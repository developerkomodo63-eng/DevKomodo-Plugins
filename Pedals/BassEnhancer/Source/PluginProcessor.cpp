#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout BassEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FOCUS", 1 }, "Focus",
        juce::NormalisableRange<float> { 60.0f, 300.0f, 0.0f, 0.5f }, 120.0f));

    // Amount of upper-harmonic content synthesized (via rectification) from
    // whatever's below Focus. This is what makes the bass audible/felt on
    // small speakers that physically can't reproduce the fundamental --
    // the ear infers the missing low note from its harmonics.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "HARMONICS", 1 }, "Harmonics", 0.0f, 10.0f, 4.0f));

    // Note-envelope-controlled filter sweep: as a note decays, the low
    // band's own highpass cutoff rises, progressively shaving sub-rumble
    // off the tail. This modulates a FILTER FREQUENCY directly from the
    // envelope, not a gain stage -- the low end tightens as it fades
    // instead of just getting quieter.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIGHT", 1 }, "Tight", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

BassEnhancerAudioProcessor::BassEnhancerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

BassEnhancerAudioProcessor::~BassEnhancerAudioProcessor()
{
}

void BassEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.lowLP.prepare (spec);
        c.highHP.prepare (spec);
        c.harmonicHP.prepare (spec);
        c.tightHP.prepare (spec);
        c.lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        c.highHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.harmonicHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 96.0f, 0.707f);
        c.tightHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0f, 0.707f);
        c.reset();
    }

    // Single envelope tracking the low band's own level, used to steer
    // the Tight filter's cutoff -- loud/attacking = cutoff stays low
    // (full low end), decaying/quiet = cutoff rises (tightens).
    levelEnvCoeff = std::exp (-1.0f / (0.120f * (float) sampleRate));
    // Peak-hold-style reference: rises fast to catch each new note's
    // peak, decays slowly so it stays a stable reference as the note
    // fades. levelEnv's decay relative to THIS is what "Tight" now
    // reacts to, instead of comparing levelEnv to a fixed absolute
    // number that could sit above or below typical signal levels
    // depending on gain staging.
    slowLevelAttackCoeff  = std::exp (-1.0f / (0.020f * (float) sampleRate));
    slowLevelReleaseCoeff = std::exp (-1.0f / (1.500f * (float) sampleRate));
}

void BassEnhancerAudioProcessor::releaseResources()
{
}

bool BassEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void BassEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
#if defined (DEVKOMODO_DEMO_BUILD)
    if (devkomodo::demoExpired (getSampleRate(), buffer.getNumSamples()))
    {
        buffer.clear();
        return;
    }
#endif

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float focus         = apvts.getRawParameterValue ("FOCUS")->load();
    const float harmonicsNorm = apvts.getRawParameterValue ("HARMONICS")->load() / 10.0f;
    const float tightNorm     = apvts.getRawParameterValue ("TIGHT")->load()     / 10.0f;
    const float mix           = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain    = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    for (auto& c : channels)
    {
        c.lowLP.setCutoffFrequency (focus);
        c.highHP.setCutoffFrequency (focus);
        c.harmonicHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            getSampleRate(), juce::jmax (40.0f, focus * 0.8f), 0.707f);
    }

    // Every dB of Tight raises how far the cutoff can climb once a note
    // has decayed, up to roughly a third of Focus.
    const float maxTightCutoffOffset = focus * 0.35f * tightNorm;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float low        = c.lowLP.processSample (0, dry);
            const float aboveFocus = c.highHP.processSample (0, dry);

            const float rectified   = std::abs (low);
            const float harmonicRaw = c.harmonicHP.processSample (rectified);
            const float harmonicOut = harmonicRaw * harmonicsNorm * 3.0f;

            // Track the low band's own envelope (single follower, slow
            // enough to represent "how alive is this note right now"
            // rather than catching individual cycles).
            const float absLow = std::abs (low);
            c.levelEnv = absLow + levelEnvCoeff * (c.levelEnv - absLow);
            const float slowCoeff = (absLow > c.slowLevelEnv) ? slowLevelAttackCoeff : slowLevelReleaseCoeff;
            c.slowLevelEnv = absLow + slowCoeff * (c.slowLevelEnv - absLow);

            // Loud/fresh note -> levelEnv close to its own recent peak ->
            // target cutoff near 30Hz (full low end). Decayed/quiet ->
            // levelEnv well below that peak -> cutoff rises toward
            // Focus*0.35 above the floor (tightens up). Comparing against
            // the note's OWN peak (rather than a fixed absolute number)
            // means this reacts consistently no matter how hot or quiet
            // the incoming signal runs.
            const float relativeLevel = c.levelEnv / juce::jmax (c.slowLevelEnv, 0.001f);
            const float decayAmount = 1.0f - juce::jlimit (0.0f, 1.0f, relativeLevel);
            const float targetCutoff = 30.0f + maxTightCutoffOffset * decayAmount;
            // One-pole smoothing on the cutoff itself so it glides rather
            // than jumps. Recomputing IIR coefficients involves trig calls,
            // which is too expensive to do every single sample -- this
            // only recalculates every 32 samples (~0.7ms at 44.1kHz),
            // which is still far faster than this filter needs to move.
            c.tightCutoffSmoothed += (targetCutoff - c.tightCutoffSmoothed) * 0.002f;
            if (--c.coeffUpdateCounter <= 0)
            {
                c.coeffUpdateCounter = 32;
                c.tightHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                    getSampleRate(), juce::jlimit (20.0f, juce::jmax (21.0f, focus), c.tightCutoffSmoothed), 0.707f);
            }
            const float lowTightened = c.tightHP.processSample (low);
            const float lowOut = low * (1.0f - tightNorm) + lowTightened * tightNorm;

            const float enhanced = lowOut + harmonicOut + aboveFocus;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* BassEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (94, 201, 188));
}

void BassEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BassEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassEnhancerAudioProcessor();
}
