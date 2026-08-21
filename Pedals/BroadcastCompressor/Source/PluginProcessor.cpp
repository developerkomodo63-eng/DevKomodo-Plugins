#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout BroadcastCompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "COMPRESSION", 1 }, "Compression", 0.0f, 10.0f, 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Output Level", -12.0f, 24.0f, 6.0f));

    return { params.begin(), params.end() };
}

BroadcastCompressorAudioProcessor::BroadcastCompressorAudioProcessor()
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

BroadcastCompressorAudioProcessor::~BroadcastCompressorAudioProcessor()
{
}

void BroadcastCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    envelopeDb = -100.0f;
    compressionSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    compressionBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    compressionSmoothed.setCurrentAndTargetValue (0.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);

    // tiempos fijos tipo opto: ataque moderado (no clava transitorios),
    // release largo (suaviza, "invisible", tipico de compresion de radio/vivo)
    attackCoeff  = std::exp (-1.0f / (0.015f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.500f * (float) sampleRate));
}

void BroadcastCompressorAudioProcessor::releaseResources()
{
}

bool BroadcastCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void BroadcastCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float compression = apvts.getRawParameterValue ("COMPRESSION")->load();
    const float levelDb     = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain  = juce::Decibels::decibelsToGain (levelDb);
    compressionSmoothed.setTargetValue (compression);
    outputGainSmoothed.setTargetValue (outputGain);
    jassert (numSamples <= (int) compressionBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        compressionBuffer[(size_t) sample] = compressionSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    // un solo knob mapea threshold y ratio juntos (0 = casi sin comprimir,
    // 10 = bastante agresivo), como el "amount" de un compresor de un solo
    // control
    constexpr float kneeWidth = 8.0f; // rodilla suave, mas musical que un hard-knee

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);
        const float peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);

        const float envCoeff = (peakDb > envelopeDb) ? attackCoeff : releaseCoeff;
        envelopeDb = peakDb + envCoeff * (envelopeDb - peakDb);

        float gainReductionDb = 0.0f;
        const float smoothedCompression = compressionBuffer[(size_t) sample];
        const float smoothedThreshold = juce::jmap (smoothedCompression, 0.0f, 10.0f, -6.0f, -36.0f);
        const float smoothedRatio = juce::jmap (smoothedCompression, 0.0f, 10.0f, 1.5f, 8.0f);
        const float kneeLow = smoothedThreshold - kneeWidth * 0.5f;
        const float kneeHigh = smoothedThreshold + kneeWidth * 0.5f;

        if (envelopeDb > kneeHigh)
        {
            const float overDb = envelopeDb - smoothedThreshold;
            gainReductionDb = overDb - overDb / smoothedRatio;
        }
        else if (envelopeDb > kneeLow)
        {
            // interpolacion cuadratica dentro de la rodilla (formula
            // estandar de soft-knee)
            const float x = envelopeDb - kneeLow;
            gainReductionDb = ((1.0f / smoothedRatio - 1.0f) * x * x) / (2.0f * kneeWidth);
        }

        const float gain = juce::Decibels::decibelsToGain (-gainReductionDb);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] = channelData[sample] * gain * outputGainBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* BroadcastCompressorAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (72, 144, 222));
}

void BroadcastCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BroadcastCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BroadcastCompressorAudioProcessor();
}
