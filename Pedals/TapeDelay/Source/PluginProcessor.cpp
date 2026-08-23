#include "PluginProcessor.h"
#include "../../Common/TempoSync.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout TapeDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIME", 1 }, "Time",
        juce::NormalisableRange<float> { 30.0f, 1500.0f, 0.0f, 0.35f }, 350.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 }, "Feedback", 0.0f, 0.92f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WOWFLUTTER", 1 }, "Wow & Flutter", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SATURATION", 1 }, "Saturation", 0.0f, 1.0f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone",
        juce::NormalisableRange<float> { 1000.0f, 10000.0f, 0.0f, 0.4f }, 4500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.35f));

        DevKomodoTempoSync::addParameters (params, 4);

    return { params.begin(), params.end() };
}

TapeDelayAudioProcessor::TapeDelayAudioProcessor()
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

TapeDelayAudioProcessor::~TapeDelayAudioProcessor()
{
}

void TapeDelayAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    wowPhase = flutterPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxDelaySeconds * sampleRate) + 4;
    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
    delaySmoothed.reset (sampleRate, 0.02);
    feedbackSmoothed.reset (sampleRate, 0.02);
    saturationSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    delayBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    feedbackBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    saturationBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);

    toneFilterL.reset();
    toneFilterR.reset();
}

void TapeDelayAudioProcessor::releaseResources()
{
}

bool TapeDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float TapeDelayAudioProcessor::tapeSaturate (float x) noexcept
{
    return std::tanh (x * 1.4f) / std::tanh (1.4f);
}

void TapeDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    float timeMs     = apvts.getRawParameterValue ("TIME")->load();
    {
        const bool tempoSynced = apvts.getRawParameterValue ("TEMPOSYNC")->load() > 0.5f;
        const int noteDivIndex = (int) apvts.getRawParameterValue ("NOTEDIV")->load();
        timeMs = DevKomodoTempoSync::resolveMilliseconds (*this, timeMs, tempoSynced, noteDivIndex, 30.0f, 1500.0f);
    }
    const float feedback   = apvts.getRawParameterValue ("FEEDBACK")->load();
    const float wowFlutter = apvts.getRawParameterValue ("WOWFLUTTER")->load();
    const float saturation = apvts.getRawParameterValue ("SATURATION")->load();
    const float toneHz     = apvts.getRawParameterValue ("TONE")->load();
    const float mix        = apvts.getRawParameterValue ("MIX")->load();
    const float delaySamplesTarget = (timeMs / 1000.0f) * (float) sampleRate;
    delaySmoothed.setTargetValue (delaySamplesTarget);
    feedbackSmoothed.setTargetValue (feedback);
    saturationSmoothed.setTargetValue (saturation);
    mixSmoothed.setTargetValue (mix);
    // jassert-only bounds checks are compiled out entirely in Release
    // builds, so they gave zero real protection: if a host/exporter uses
    // a block size larger than what prepareToPlay() originally sized
    // these buffers for (this happens with some DAWs' offline bounce/
    // export, which can use a different block size than realtime
    // playback), the per-sample smoothing loop below would write past
    // the end of these vectors -- a real heap buffer overflow, not just
    // a debug-mode warning. Actually growing the buffers here fixes it
    // for any block size the host throws at us.
        if (numSamples > (int) delayBuffer.size()) delayBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) feedbackBuffer.size()) feedbackBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) saturationBuffer.size()) saturationBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        delayBuffer[(size_t) sample] = delaySmoothed.getNextValue();
        feedbackBuffer[(size_t) sample] = feedbackSmoothed.getNextValue();
        saturationBuffer[(size_t) sample] = saturationSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
    }

    auto toneCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, toneHz);
    *toneFilterL.coefficients = toneCoeffs;
    *toneFilterR.coefficients = toneCoeffs;

    constexpr float wowRateHz = 0.7f, flutterRateHz = 7.0f;
    constexpr float wowMaxMs = 3.0f, flutterMaxMs = 0.8f;
    const float wowInc = wowRateHz / (float) sampleRate;
    const float flutterInc = flutterRateHz / (float) sampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        wowPhase += wowInc;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += flutterInc;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;

        const float wobbleMs = wowFlutter * (wowMaxMs * std::sin (juce::MathConstants<float>::twoPi * wowPhase)
                                            + flutterMaxMs * std::sin (juce::MathConstants<float>::twoPi * flutterPhase));
        const float delayMs = juce::jmax (5.0f, delayBuffer[(size_t) sample] / (float) sampleRate * 1000.0f + wobbleMs);
        const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            auto& line = lines[(size_t) channel];
            const int lineLength = (int) line.size();
            int& wp = writePos[(size_t) channel];
            auto& toneFilter = (channel == 0) ? toneFilterL : toneFilterR;

            float readPosF = (float) wp - delaySamples;
            while (readPosF < 0.0f)
                readPosF += (float) lineLength;

            const int readIdx0 = (int) readPosF;
            const int readIdx1 = (readIdx0 + 1) % lineLength;
            const float frac = readPosF - (float) readIdx0;

            const float delayedSample = line[(size_t) readIdx0] * (1.0f - frac) + line[(size_t) readIdx1] * frac;

            // solo lo que vuelve al buffer se satura y se oscurece -- por
            // eso cada repeticion sucesiva se degrada un poco mas
            float feedbackSignal = toneFilter.processSample (delayedSample);
            const float smoothedSaturation = saturationBuffer[(size_t) sample];
            feedbackSignal = feedbackSignal * (1.0f - smoothedSaturation)
                           + tapeSaturate (feedbackSignal) * smoothedSaturation;

            const float input = channelData[sample];
            line[(size_t) wp] = input + feedbackSignal * feedbackBuffer[(size_t) sample];
            wp = (wp + 1) % lineLength;

            const float smoothedMix = mixBuffer[(size_t) sample];
            channelData[sample] = input * (1.0f - smoothedMix) + delayedSample * smoothedMix;
        }
    }
}

juce::AudioProcessorEditor* TapeDelayAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (94, 201, 188));
}

void TapeDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TapeDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeDelayAudioProcessor();
}
