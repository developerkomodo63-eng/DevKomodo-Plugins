#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout DrumEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CROSSOVER", 1 }, "Crossover",
        juce::NormalisableRange<float> { 200.0f, 2000.0f, 0.0f, 0.4f }, 600.0f));

    // Dynamic low-band boost, scaled by how strong the detected transient
    // is right now -- silent between hits, present on the attack.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PUNCH", 1 }, "Punch", 0.0f, 10.0f, 4.0f));

    // Synthesized sub layer: the low band, filtered much harder and
    // saturated, for real low-end weight rather than just "more bass".
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUB", 1 }, "Sub", 0.0f, 10.0f, 3.0f));

    // High-band harmonic excitement, gated by the transient so cymbal/
    // hihat sustain doesn't pick up constant grit -- only the attack does.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CRACK", 1 }, "Crack", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

DrumEnhancerAudioProcessor::DrumEnhancerAudioProcessor()
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

DrumEnhancerAudioProcessor::~DrumEnhancerAudioProcessor()
{
}

void DrumEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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
        c.subLP.prepare (spec);
        c.lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        c.highHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        // Fixed hard lowpass for the synthesized sub layer -- this stays
        // well below the adjustable crossover on purpose, it's meant to
        // isolate just the "boom" fundamental, not track the Crossover knob.
        c.subLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 90.0f, 0.707f);
        c.reset();
    }

    // Transient envelope pair: a fast follower that jumps up almost
    // instantly on a hit, and a slow follower that tracks the recent
    // "steady" loudness. The gap between them (fast minus slow) is the
    // transient-detection signal every dynamic parameter below reacts to.
    const auto sr = (float) sampleRate;
    fastAttackCoeff  = std::exp (-1.0f / (0.0008f * sr));   // ~0.8ms
    fastReleaseCoeff = std::exp (-1.0f / (0.030f  * sr));   // ~30ms
    slowAttackCoeff  = std::exp (-1.0f / (0.060f  * sr));   // ~60ms
    slowReleaseCoeff = std::exp (-1.0f / (0.350f  * sr));   // ~350ms
}

void DrumEnhancerAudioProcessor::releaseResources()
{
}

bool DrumEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DrumEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float crossover = apvts.getRawParameterValue ("CROSSOVER")->load();
    const float punchNorm = apvts.getRawParameterValue ("PUNCH")->load() / 10.0f;
    const float subNorm   = apvts.getRawParameterValue ("SUB")->load()   / 10.0f;
    const float crackNorm = apvts.getRawParameterValue ("CRACK")->load() / 10.0f;
    const float mix        = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb    = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    for (auto& c : channels)
    {
        c.lowLP.setCutoffFrequency (crossover);
        c.highHP.setCutoffFrequency (crossover);
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];
            const float absDry = std::abs (dry);

            // Transient envelope: fast follower jumps on the hit, slow
            // follower tracks the recent average. The (fast - slow) gap,
            // normalised against the recent level, spikes to ~1 right at
            // a hit and decays back to ~0 during sustain/silence.
            const float fastCoeff = (absDry > c.fastEnv) ? fastAttackCoeff : fastReleaseCoeff;
            c.fastEnv = absDry + fastCoeff * (c.fastEnv - absDry);
            const float slowCoeff = (absDry > c.slowEnv) ? slowAttackCoeff : slowReleaseCoeff;
            c.slowEnv = absDry + slowCoeff * (c.slowEnv - absDry);
            const float transient = juce::jlimit (0.0f, 1.0f,
                (c.fastEnv - c.slowEnv) * 4.0f / juce::jmax (c.slowEnv, 0.02f));

            const float low  = c.lowLP.processSample (0, dry);
            const float high = c.highHP.processSample (0, dry);

            // PUNCH: extra low-band gain that only appears when there's
            // an active transient -- silent between hits instead of a
            // constant static boost.
            const float punchDrive = 1.0f + punchNorm * transient * 4.0f;
            const float lowOut = low * (1.0f - punchNorm) + std::tanh (low * punchDrive) * punchNorm;

            // SUB: a hard-filtered, saturated low layer for real kick
            // weight. Always tracks the transient a little too, so it
            // doesn't just add a static hum under everything.
            const float subRaw = c.subLP.processSample (low);
            const float subSat = std::tanh (subRaw * (2.0f + transient * 1.5f));
            const float subOut = subSat * subNorm;

            // CRACK: high-band excitement gated by the transient, so
            // cymbal/hihat sustain stays clean and only the attack gets
            // the extra harmonic snap.
            const float crackDrive = 1.0f + crackNorm * transient * 6.0f;
            const float highOut = high * (1.0f - crackNorm) + std::tanh (high * crackDrive) * crackNorm;

            const float enhanced = lowOut + subOut + highOut;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* DrumEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 159, 109));
}

void DrumEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DrumEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumEnhancerAudioProcessor();
}
