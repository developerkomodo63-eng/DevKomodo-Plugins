#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout AutoSwellAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SWELLTIME", 1 }, "Swell Time",
        juce::NormalisableRange<float> { 50.0f, 3000.0f, 0.0f, 0.4f }, 500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "THRESHOLD", 1 }, "Threshold", -60.0f, -20.0f, -40.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

AutoSwellAudioProcessor::AutoSwellAudioProcessor()
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

AutoSwellAudioProcessor::~AutoSwellAudioProcessor()
{
}

void AutoSwellAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    sampleRate = sampleRateIn;
    swellTimeSmoothed.reset (sampleRate, 0.02);
    thresholdSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    swellTimeBuffer.assign ((size_t) samplesPerBlock, 500.0f);
    thresholdBuffer.assign ((size_t) samplesPerBlock, -40.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    swellTimeSmoothed.setCurrentAndTargetValue (500.0f);
    thresholdSmoothed.setCurrentAndTargetValue (-40.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);

    envelopeState = 0.0f;
    lastEnvelope = 0.0f;
    swellGain = 1.0f; // arranca "abierto" para no silenciar el primer instante antes de la primera nota

    envAttackCoeff  = std::exp (-1.0f / (0.002f * (float) sampleRate));
    envReleaseCoeff = std::exp (-1.0f / (0.150f * (float) sampleRate));
}

void AutoSwellAudioProcessor::releaseResources()
{
}

bool AutoSwellAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AutoSwellAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float swellTimeMs = apvts.getRawParameterValue ("SWELLTIME")->load();
    const float thresholdDb = apvts.getRawParameterValue ("THRESHOLD")->load();
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb     = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain  = juce::Decibels::decibelsToGain (levelDb);
    swellTimeSmoothed.setTargetValue (swellTimeMs);
    thresholdSmoothed.setTargetValue (thresholdDb);
    mixSmoothed.setTargetValue (mix);
    outputGainSmoothed.setTargetValue (outputGain);
    jassert (numSamples <= (int) swellTimeBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        swellTimeBuffer[(size_t) sample] = swellTimeSmoothed.getNextValue();
        thresholdBuffer[(size_t) sample] = thresholdSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float thresholdLinear = juce::Decibels::decibelsToGain (thresholdBuffer[(size_t) sample]);
        const float swellCoeff = std::exp (-1.0f / (swellTimeBuffer[(size_t) sample] / 1000.0f * (float) sampleRate));
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);
        const float envCoeff = (peak > envelopeState) ? envAttackCoeff : envReleaseCoeff;
        envelopeState = peak + envCoeff * (envelopeState - peak);

        // flanco ascendente desde silencio: arranca una nota nueva
        if (lastEnvelope < thresholdLinear && envelopeState >= thresholdLinear)
            swellGain = 0.0f;
        lastEnvelope = envelopeState;

        swellGain += (1.0f - swellGain) * (1.0f - swellCoeff);

        const float smoothedMix = mixBuffer[(size_t) sample];
        const float blendedGain = (1.0f - smoothedMix) + smoothedMix * swellGain;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] = channelData[sample] * blendedGain * outputGainBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* AutoSwellAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (188, 150, 255));
}

void AutoSwellAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AutoSwellAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoSwellAudioProcessor();
}
