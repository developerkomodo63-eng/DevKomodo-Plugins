#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout GuitarEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Adaptive mud reduction: digs into the ~200-500Hz box-resonance
    // region harder only when that band is actually dense (strummed
    // chords, doubled/layered tracks), and steps back for open single-
    // note lines instead of thinning them out like a static EQ cut would.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CLARITY", 1 }, "Clarity", 0.0f, 10.0f, 5.0f));

    // Loudness-following presence: tracks a slow overall RMS level (how
    // hard you're digging in), not individual pick-attack transients.
    // Play harder and the top end opens up gradually over the phrase;
    // back off and it relaxes back down.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PRESENCE", 1 }, "Presence", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

GuitarEnhancerAudioProcessor::GuitarEnhancerAudioProcessor()
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

GuitarEnhancerAudioProcessor::~GuitarEnhancerAudioProcessor()
{
}

void GuitarEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.mudBand.prepare (spec);
        c.presenceHP.prepare (spec);
        c.mudBand.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 350.0f, 1.1f);
        c.presenceHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.presenceHP.setCutoffFrequency (2500.0f);
        c.reset();
    }

    const auto sr = (float) sampleRate;
    mudEnvCoeff = std::exp (-1.0f / (0.015f * sr));
    // Deliberately slow/heavy -- this is meant to average out over a
    // phrase (roughly half a second), not react to individual notes.
    rmsEnvCoeff = std::exp (-1.0f / (0.500f * sr));
    // Reference baseline for Presence: how loud you've been playing over
    // the last ~2.5s. Comparing the current (500ms) envelope against this
    // instead of a fixed number means "digging in" always means
    // something, regardless of the input's absolute gain staging.
    broadbandSlowCoeff = std::exp (-1.0f / (2.5f * sr));
}

void GuitarEnhancerAudioProcessor::releaseResources()
{
}

bool GuitarEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GuitarEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float clarityNorm  = apvts.getRawParameterValue ("CLARITY")->load()  / 10.0f;
    const float presenceNorm = apvts.getRawParameterValue ("PRESENCE")->load() / 10.0f;
    const float mix          = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain   = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    // Both ratios below are self-calibrating against the signal's own
    // level, not a fixed absolute-amplitude number -- the earlier version
    // used hard-coded thresholds (0.06, 0.10) that were guessed without
    // being able to listen, and sat above typical signal levels for at
    // least some gain-staging setups, making the effect nearly silent no
    // matter how the knobs were set. This version reacts consistently
    // regardless of how hot or quiet the incoming signal runs.
    static constexpr float mudBaselineRatio = 0.35f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];
            const float absDry = std::abs (dry);

            // Adaptive mud reduction: compares the 350Hz band's own
            // envelope against a same-timescale broadband reference, so
            // it reacts to how DENSE the playing is right now (relative
            // to the signal's own overall level) rather than an absolute
            // number that may or may not mean anything for this input.
            const float mud = c.mudBand.processSample (dry);
            const float absMud = std::abs (mud);
            c.mudFastEnv = absMud + mudEnvCoeff * (c.mudFastEnv - absMud);
            c.broadbandFastEnv = absDry + mudEnvCoeff * (c.broadbandFastEnv - absDry);
            const float mudRatio = c.mudFastEnv / juce::jmax (c.broadbandFastEnv, 0.001f);
            const float excess = juce::jmax (0.0f, mudRatio - mudBaselineRatio);
            const float reduction = clarityNorm * juce::jmin (1.0f, excess * 3.0f);
            const float mudCut = mud * reduction;

            // Presence follows the ratio of a fast (500ms) envelope
            // against a much slower (2.5s) one -- how hard you're
            // playing right now relative to your own recent baseline.
            // At steady dynamics this ratio sits near 1.0, so Presence
            // is always doing something audible when it's turned up,
            // not just during dynamic swells.
            c.rmsEnv = absDry + rmsEnvCoeff * (c.rmsEnv - absDry);
            c.broadbandSlowEnv = absDry + broadbandSlowCoeff * (c.broadbandSlowEnv - absDry);
            const float loudnessAmount = juce::jlimit (0.0f, 2.0f,
                c.rmsEnv / juce::jmax (c.broadbandSlowEnv, 0.001f));
            const float presenceBand = c.presenceHP.processSample (0, dry);
            const float presenceGainExtra = presenceNorm * loudnessAmount * 1.6f;

            // At Clarity=0 and Presence=0 this reconstructs dry exactly.
            const float enhanced = dry - mudCut + presenceBand * presenceGainExtra;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* GuitarEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (135, 195, 110));
}

void GuitarEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GuitarEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarEnhancerAudioProcessor();
}
