#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout VocalEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Boosts low-mid body, but only routed to content the zero-crossing
    // detector currently reads as voiced (vowels/sustained tone) --
    // consonants and breath don't get thickened.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WARMTH", 1 }, "Warmth", 0.0f, 10.0f, 4.0f));

    // Boosts high-end air, routed to content read as unvoiced (breath,
    // sibilance-adjacent consonants) -- sustained vowels don't get hissy.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AIR", 1 }, "Air", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

VocalEnhancerAudioProcessor::VocalEnhancerAudioProcessor()
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

VocalEnhancerAudioProcessor::~VocalEnhancerAudioProcessor()
{
}

void VocalEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.warmthBand.prepare (spec);
        c.airHP.prepare (spec);
        c.warmthBand.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 500.0f, 0.9f);
        c.airHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.airHP.setCutoffFrequency (8000.0f);
        c.reset();
    }

    // Averaging window for the zero-crossing rate: short enough to react
    // within a syllable, long enough not to flicker sample-to-sample.
    zcrEnvCoeff = std::exp (-1.0f / (0.012f * (float) sampleRate));
}

void VocalEnhancerAudioProcessor::releaseResources()
{
}

bool VocalEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void VocalEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float warmthNorm = apvts.getRawParameterValue ("WARMTH")->load() / 10.0f;
    const float airNorm    = apvts.getRawParameterValue ("AIR")->load()    / 10.0f;
    const float mix        = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    // Crossing-rate thresholds, expressed as a fraction of samples that
    // are a sign change. A sung vowel's fundamental (roughly 80-350Hz)
    // sits well under the low threshold; broadband consonant/breath
    // noise sits well over the high one. Between the two is a soft
    // transition rather than a hard switch.
    static constexpr float lowThreshold  = 0.012f;
    static constexpr float highThreshold = 0.050f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float currentSign = dry >= 0.0f ? 1.0f : -1.0f;
            const float crossed = (currentSign != c.lastSign) ? 1.0f : 0.0f;
            c.lastSign = currentSign;
            c.zcrEnv = crossed + zcrEnvCoeff * (c.zcrEnv - crossed);

            const float breathAmount = juce::jlimit (0.0f, 1.0f,
                (c.zcrEnv - lowThreshold) / (highThreshold - lowThreshold));
            const float voicedAmount = 1.0f - breathAmount;

            const float warmthBand = c.warmthBand.processSample (dry);
            const float airBand    = c.airHP.processSample (0, dry);

            const float warmthGain = warmthNorm * voicedAmount * 2.0f;
            const float airGain    = airNorm * breathAmount * 2.2f;

            // At Warmth=0 and Air=0 this reconstructs dry exactly.
            const float enhanced = dry + warmthBand * warmthGain + airBand * airGain;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* VocalEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 150, 185));
}

void VocalEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VocalEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalEnhancerAudioProcessor();
}
