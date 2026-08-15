#include "PluginProcessor.h"
#include "../../Common/fast_tanh.h"

juce::AudioProcessorValueTreeState::ParameterLayout LimiterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "THRESH", 1 }, "Threshold", -24.0f, 0.0f, -3.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "RELEASE", 1 }, "Release", 1.0f, 200.0f, 60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MAKEUP", 1 }, "Makeup", -12.0f, 12.0f, 0.0f));
    return { params.begin(), params.end() };
}

LimiterAudioProcessor::LimiterAudioProcessor()
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

LimiterAudioProcessor::~LimiterAudioProcessor() {}

void LimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    fs = sampleRate;
}

void LimiterAudioProcessor::releaseResources() {}

bool LimiterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void LimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float threshDb = apvts.getRawParameterValue ("THRESH")->load();
    const float threshLin = juce::Decibels::decibelsToGain (threshDb);
    const float makeupDb = apvts.getRawParameterValue ("MAKEUP")->load();
    const float makeup = juce::Decibels::decibelsToGain (makeupDb);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float in = data[i];
            // simple soft limiter: tanh saturation scaled by threshold
            float x = in * makeup;
            if (std::abs (x) > threshLin)
                x = fast_tanh (x / threshLin) * threshLin;
            data[i] = x;
        }
    }
}

void LimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void LimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LimiterAudioProcessor(); }
