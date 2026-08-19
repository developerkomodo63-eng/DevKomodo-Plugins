#include "PluginProcessor.h"
#include "DevKomodoUI.h"


juce::AudioProcessorValueTreeState::ParameterLayout ConsoleEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LOW", 1 }, "Low Gain", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MID", 1 }, "Mid Gain", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "HIGH", 1 }, "High Gain", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LOWF", 1 }, "Low Freq", 40.0f, 400.0f, 120.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIDF", 1 }, "Mid Freq", 400.0f, 4000.0f, 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "HIGHF", 1 }, "High Freq", 2000.0f, 16000.0f, 8000.0f));
    return { params.begin(), params.end() };
}

ConsoleEQAudioProcessor::ConsoleEQAudioProcessor()
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

ConsoleEQAudioProcessor::~ConsoleEQAudioProcessor() {}

void ConsoleEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    fs = sampleRate;
    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    lowFilters.clear();
    midFilters.clear();
    highFilters.clear();
    lowFilters.reserve ((size_t) numChannels);
    midFilters.reserve ((size_t) numChannels);
    highFilters.reserve ((size_t) numChannels);

    juce::dsp::ProcessSpec spec { fs, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowFilters.push_back (std::make_unique<juce::dsp::IIR::Filter<float>>());
        midFilters.push_back (std::make_unique<juce::dsp::IIR::Filter<float>>());
        highFilters.push_back (std::make_unique<juce::dsp::IIR::Filter<float>>());

        lowFilters[(size_t) ch]->prepare (spec);
        midFilters[(size_t) ch]->prepare (spec);
        highFilters[(size_t) ch]->prepare (spec);
    }
}

void ConsoleEQAudioProcessor::releaseResources() {}

bool ConsoleEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ConsoleEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = juce::jmin (getTotalNumInputChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();

    const float lowDb = apvts.getRawParameterValue ("LOW")->load();
    const float midDb = apvts.getRawParameterValue ("MID")->load();
    const float highDb = apvts.getRawParameterValue ("HIGH")->load();
    const float lowF = apvts.getRawParameterValue ("LOWF")->load();
    const float midF = apvts.getRawParameterValue ("MIDF")->load();
    const float highF = apvts.getRawParameterValue ("HIGHF")->load();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& lf = *lowFilters[(size_t) ch];
        auto& mf = *midFilters[(size_t) ch];
        auto& hf = *highFilters[(size_t) ch];

        *lf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (fs, lowF, 0.7f, juce::Decibels::decibelsToGain (lowDb));
        *mf.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, midF, 1.0f, juce::Decibels::decibelsToGain (midDb));
        *hf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, highF, 0.7f, juce::Decibels::decibelsToGain (highDb));

        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = hf.processSample (mf.processSample (lf.processSample (data[i])));
    }
}

void ConsoleEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (st.createXml());
    copyXmlToBinary (*xml, destData);
}

void ConsoleEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* ConsoleEQAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (250, 204, 100));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ConsoleEQAudioProcessor(); }
