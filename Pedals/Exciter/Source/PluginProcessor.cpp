#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout ExciterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FREQUENCY", 1 }, "Frequency",
        juce::NormalisableRange<float> { 1500.0f, 8000.0f, 0.0f, 0.4f }, 3500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "AMOUNT", 1 }, "Amount", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

ExciterAudioProcessor::ExciterAudioProcessor()
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

ExciterAudioProcessor::~ExciterAudioProcessor()
{
}

void ExciterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare (spec);
    hpFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    amountSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    amountBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

void ExciterAudioProcessor::releaseResources()
{
}

bool ExciterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void ExciterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float freq     = apvts.getRawParameterValue ("FREQUENCY")->load();
    const float amount   = apvts.getRawParameterValue ("AMOUNT")->load();
    const float mix      = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb  = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);
    amountSmoothed.setTargetValue (amount);
    mixSmoothed.setTargetValue (mix);
    outputGainSmoothed.setTargetValue (outputGain);
    jassert (numSamples <= (int) amountBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        amountBuffer[(size_t) sample] = amountSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    hpFilter.setCutoffFrequency (freq);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];
            const float high = hpFilter.processSample (channel, dry);
            const float smoothedAmount = amountBuffer[(size_t) sample];
            const float smoothedMix = mixBuffer[(size_t) sample];
            const float driveStage = 1.0f + smoothedAmount * 8.0f;

            // saturacion impar (genera armonicos altos, "brillo") escalada
            // por Amount; se normaliza para que subir el drive no baje el
            // volumen percibido de la excitacion
            const float excited = std::tanh (high * driveStage) / juce::jmax (driveStage, 1.0f);

            channelData[sample] = (dry + excited * smoothedAmount * smoothedMix)
                                * outputGainBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* ExciterAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (240, 190, 72));
}

void ExciterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ExciterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ExciterAudioProcessor();
}
