#include "PluginProcessor.h"
#include "../../Common/fast_tanh.h"

juce::AudioProcessorValueTreeState::ParameterLayout CleanUpProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "GATE", 1 }, "Gate Threshold", -60.0f, 0.0f, -48.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DEESSF", 1 }, "De-esser Freq", 2000.0f, 8000.0f, 5000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DEESSA", 1 }, "De-esser Amount", 0.0f, 1.0f, 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TRANS", 1 }, "Transient", 0.0f, 1.0f, 0.5f));
    return { params.begin(), params.end() };
}

CleanUpProAudioProcessor::CleanUpProAudioProcessor()
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

CleanUpProAudioProcessor::~CleanUpProAudioProcessor() {}

void CleanUpProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    fs = sampleRate;
}

void CleanUpProAudioProcessor::releaseResources() {}

bool CleanUpProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CleanUpProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float gateDb = apvts.getRawParameterValue ("GATE")->load();
    const float gateThresh = juce::Decibels::decibelsToGain (gateDb);
    const float deFreq = apvts.getRawParameterValue ("DEESSF")->load();
    const float deAmt = apvts.getRawParameterValue ("DEESSA")->load();
    const float trans = apvts.getRawParameterValue ("TRANS")->load();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float in = data[i];
            // gate (very simple): attenuate low-level signals
            if (std::abs (in) < gateThresh)
                in *= 0.08f;
            // transient: simple soft clip boost on peaks
            float boosted = in * (1.0f + trans * 0.8f);
            boosted = fast_tanh (boosted);
            // de-esser placeholder: slight attenuation above freq (cheap) -- not a real de-esser yet
            // keep as pass-through for now but scaled by deAmt (placeholder)
            float out = juce::jlimit (-1.0f, 1.0f, boosted * (1.0f - deAmt * 0.12f));
            data[i] = out;
        }
    }
}

void CleanUpProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void CleanUpProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CleanUpProAudioProcessor(); }
