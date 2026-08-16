#include "PluginProcessor.h"
#include "DevKomodoUI.h"

#include "../../Common/fast_tanh.h"

juce::AudioProcessorValueTreeState::ParameterLayout ConsoleDriveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 10.0f, 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "TONE", 1 }, "Tone", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));
    return { params.begin(), params.end() };
}

ConsoleDriveAudioProcessor::ConsoleDriveAudioProcessor()
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

ConsoleDriveAudioProcessor::~ConsoleDriveAudioProcessor() {}

void ConsoleDriveAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = newSampleRate;
    lowState.assign ((size_t) juce::jmax (1, getTotalNumOutputChannels()), 0.0f);
}

void ConsoleDriveAudioProcessor::releaseResources() {}

bool ConsoleDriveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ConsoleDriveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numCh = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float driveKnob = apvts.getRawParameterValue ("DRIVE")->load();
    const float drive = driveKnob * 0.1f;
    const float tone  = apvts.getRawParameterValue ("TONE")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float gain = juce::Decibels::decibelsToGain (levelDb);
    const float lowCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 180.0f / (float) sampleRate);
    const float toneTilt = tone / 12.0f;

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float low = lowState[(size_t) ch];
        for (int i = 0; i < numSamples; ++i)
        {
            const float in = data[i];
            // Console drive is intentionally subtle: low-frequency energy is
            // compressed a little more and the tone control is a true spectral
            // tilt, rather than a post-saturation volume multiplier.
            low += (in - low) * lowCoeff;
            const float high = in - low;
            const float driven = low * (1.0f + drive * 1.8f) + high * (1.0f + drive * 2.4f);
            float shaped = driven / (1.0f + 0.22f * std::abs (driven));
            shaped += toneTilt * (high * 0.35f - low * 0.10f);
            data[i] = shaped * gain;
        }
        lowState[(size_t) ch] = low;
    }
}

void ConsoleDriveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void ConsoleDriveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* ConsoleDriveAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (230, 126, 34));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ConsoleDriveAudioProcessor(); }
