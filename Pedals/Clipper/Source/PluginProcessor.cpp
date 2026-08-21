#include "PluginProcessor.h"
#include "DevKomodoUI.h"


juce::AudioProcessorValueTreeState::ParameterLayout ClipperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "THRESH", 1 }, "Threshold", 0.0f, 1.0f, 0.95f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MAKEUP", 1 }, "Makeup", -12.0f, 12.0f, 0.0f));
    return { params.begin(), params.end() };
}

ClipperAudioProcessor::ClipperAudioProcessor()
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

ClipperAudioProcessor::~ClipperAudioProcessor() {}

void ClipperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    fs = sampleRate;
    thresholdSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    makeupSmoothed.reset (sampleRate, 0.02);
    thresholdBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    makeupBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

void ClipperAudioProcessor::releaseResources() {}

bool ClipperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ClipperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
#if defined (DEVKOMODO_DEMO_BUILD)
    if (devkomodo::demoExpired (getSampleRate(), buffer.getNumSamples()))
    {
        buffer.clear();
        return;
    }
#endif
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float thresh = apvts.getRawParameterValue ("THRESH")->load();
    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const float makeupDb = apvts.getRawParameterValue ("MAKEUP")->load();
    const float makeup = juce::Decibels::decibelsToGain (makeupDb);
    thresholdSmoothed.setTargetValue (thresh);
    mixSmoothed.setTargetValue (mix);
    makeupSmoothed.setTargetValue (makeup);
    jassert (numSamples <= (int) thresholdBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        thresholdBuffer[(size_t) sample] = thresholdSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        makeupBuffer[(size_t) sample] = makeupSmoothed.getNextValue();
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float smoothedMakeup = makeupBuffer[(size_t) i];
            const float smoothedThreshold = thresholdBuffer[(size_t) i];
            const float smoothedMix = mixBuffer[(size_t) i];
            float in = data[i] * smoothedMakeup;
            float clipped = juce::jlimit (-smoothedThreshold, smoothedThreshold, in);
            float out = in * (1.0f - smoothedMix) + clipped * smoothedMix;
            data[i] = out;
        }
    }
}

void ClipperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void ClipperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* ClipperAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 107, 53));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ClipperAudioProcessor(); }
