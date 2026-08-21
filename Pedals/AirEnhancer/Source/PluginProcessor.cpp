#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout AirEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LOW", 1 }, "Low", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "HIGH", 1 }, "High", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -6.0f, 6.0f, 0.0f));

    return { params.begin(), params.end() };
}

AirEnhancerAudioProcessor::AirEnhancerAudioProcessor()
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

AirEnhancerAudioProcessor::~AirEnhancerAudioProcessor()
{
}

void AirEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    lowBandFilter.prepare (spec);
    lowBandFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowBandFilter.setCutoffFrequency (1200.0f);

    highBandFilter.prepare (spec);
    highBandFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highBandFilter.setCutoffFrequency (10000.0f);
    lowSmoothed.reset (sampleRate, 0.02);
    highSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    lowBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    highBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

void AirEnhancerAudioProcessor::releaseResources()
{
}

bool AirEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AirEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float lowAmt   = apvts.getRawParameterValue ("LOW")->load();
    const float highAmt  = apvts.getRawParameterValue ("HIGH")->load();
    const float levelDb  = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);
    lowSmoothed.setTargetValue (lowAmt); highSmoothed.setTargetValue (highAmt);
    outputGainSmoothed.setTargetValue (outputGain);
    jassert (numSamples <= (int) lowBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        lowBuffer[(size_t) sample] = lowSmoothed.getNextValue();
        highBuffer[(size_t) sample] = highSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float low  = lowBandFilter.processSample (channel, dry);
            const float high = highBandFilter.processSample (channel, dry);

            // rango de drive deliberadamente chico: esto es para sutileza,
            // no para un efecto obvio -- si querés algo mas marcado, ese
            // es el trabajo del Exciter de proposito general
            const float smoothedLow = lowBuffer[(size_t) sample];
            const float smoothedHigh = highBuffer[(size_t) sample];
            const float lowExcited  = std::tanh (low * (1.0f + smoothedLow * 3.0f));
            const float highExcited = std::tanh (high * (1.0f + smoothedHigh * 3.0f));

            channelData[sample] = (dry + lowExcited * smoothedLow * 0.5f + highExcited * smoothedHigh * 0.5f)
                                * outputGainBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* AirEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 214, 128));
}

void AirEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AirEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AirEnhancerAudioProcessor();
}
