#include "PluginProcessor.h"
#include "../../Common/TempoSync.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout FlangerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 0.02f, 5.0f, 0.0f, 0.4f }, 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MANUAL", 1 }, "Manual", 0.2f, 8.0f, 1.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 }, "Feedback", -0.95f, 0.95f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

        DevKomodoTempoSync::addParameters (params, 7);

    return { params.begin(), params.end() };
}

FlangerAudioProcessor::FlangerAudioProcessor()
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

FlangerAudioProcessor::~FlangerAudioProcessor()
{
}

void FlangerAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    lfoPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxLineMs / 1000.0 * sampleRate) + 4;

    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
    rateSmoothed.reset (sampleRate, 0.02);
    depthSmoothed.reset (sampleRate, 0.02);
    manualSmoothed.reset (sampleRate, 0.02);
    feedbackSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    rateBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    depthBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    manualBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    feedbackBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

void FlangerAudioProcessor::releaseResources()
{
}

bool FlangerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void FlangerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    float rateHz   = apvts.getRawParameterValue ("RATE")->load();
    {
        const bool tempoSynced = apvts.getRawParameterValue ("TEMPOSYNC")->load() > 0.5f;
        const int noteDivIndex = (int) apvts.getRawParameterValue ("NOTEDIV")->load();
        rateHz = DevKomodoTempoSync::resolveHz (*this, rateHz, tempoSynced, noteDivIndex, 0.02f, 5.0f);
    }
    const float depth    = apvts.getRawParameterValue ("DEPTH")->load();
    const float manualMs = apvts.getRawParameterValue ("MANUAL")->load();
    const float feedback = apvts.getRawParameterValue ("FEEDBACK")->load();
    const float mix      = apvts.getRawParameterValue ("MIX")->load();

    rateSmoothed.setTargetValue (rateHz);
    depthSmoothed.setTargetValue (depth);
    manualSmoothed.setTargetValue (manualMs);
    feedbackSmoothed.setTargetValue (feedback);
    mixSmoothed.setTargetValue (mix);
    jassert (numSamples <= (int) rateBuffer.size());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        rateBuffer[(size_t) sample] = rateSmoothed.getNextValue();
        depthBuffer[(size_t) sample] = depthSmoothed.getNextValue();
        manualBuffer[(size_t) sample] = manualSmoothed.getNextValue();
        feedbackBuffer[(size_t) sample] = feedbackSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
    }

    constexpr float maxModMs = 4.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float phaseInc = rateBuffer[(size_t) sample] / (float) sampleRate;
        const float smoothedDepth = depthBuffer[(size_t) sample];
        const float smoothedManual = manualBuffer[(size_t) sample];
        const float smoothedFeedback = feedbackBuffer[(size_t) sample];
        const float smoothedMix = mixBuffer[(size_t) sample];
        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        const float modMs = smoothedDepth * maxModMs * 0.5f * (1.0f + std::sin (juce::MathConstants<float>::twoPi * lfoPhase));
        const float delayMs = juce::jmax (0.1f, smoothedManual + modMs);
        const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            auto& line = lines[(size_t) channel];
            const int lineLength = (int) line.size();
            int& wp = writePos[(size_t) channel];

            float readPosF = (float) wp - delaySamples;
            while (readPosF < 0.0f)
                readPosF += (float) lineLength;

            const int readIdx0 = (int) readPosF;
            const int readIdx1 = (readIdx0 + 1) % lineLength;
            const float frac = readPosF - (float) readIdx0;

            const float wet = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;
            const float input = channelData[sample];

            line[(size_t) wp] = input + wet * smoothedFeedback;
            wp = (wp + 1) % lineLength;

            channelData[sample] = input * (1.0f - smoothedMix) + wet * smoothedMix;
        }
    }
}

juce::AudioProcessorEditor* FlangerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (123, 97, 255));
}

void FlangerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FlangerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlangerAudioProcessor();
}
