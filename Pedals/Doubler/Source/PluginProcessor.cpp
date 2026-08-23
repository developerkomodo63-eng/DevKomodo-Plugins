#include "PluginProcessor.h"
#include "../../Common/TempoSync.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout DoublerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DELAY", 1 }, "Delay",
        juce::NormalisableRange<float> { 10.0f, 40.0f, 0.0f, 0.6f }, 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 0.05f, 2.0f, 0.0f, 0.5f }, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WIDTH", 1 }, "Stereo Width", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

        DevKomodoTempoSync::addParameters (params, 1);

    return { params.begin(), params.end() };
}

DoublerAudioProcessor::DoublerAudioProcessor()
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

DoublerAudioProcessor::~DoublerAudioProcessor()
{
}

void DoublerAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    lfoPhase = 0.0f;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int lineLength = (int) (maxLineMs / 1000.0 * sampleRate) + 4;

    lines.assign ((size_t) numChannels, std::vector<float> ((size_t) lineLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
    delaySmoothed.reset (sampleRate, 0.02);
    rateSmoothed.reset (sampleRate, 0.02);
    depthSmoothed.reset (sampleRate, 0.02);
    widthSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    delayBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    rateBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    depthBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    widthBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

void DoublerAudioProcessor::releaseResources()
{
}

bool DoublerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DoublerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float delayMsBase = apvts.getRawParameterValue ("DELAY")->load();
    float rateHz      = apvts.getRawParameterValue ("RATE")->load();
    {
        const bool tempoSynced = apvts.getRawParameterValue ("TEMPOSYNC")->load() > 0.5f;
        const int noteDivIndex = (int) apvts.getRawParameterValue ("NOTEDIV")->load();
        rateHz = DevKomodoTempoSync::resolveHz (*this, rateHz, tempoSynced, noteDivIndex, 0.05f, 2.0f);
    }
    const float depth       = apvts.getRawParameterValue ("DEPTH")->load();
    const float width       = apvts.getRawParameterValue ("WIDTH")->load();
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    delaySmoothed.setTargetValue (delayMsBase);
    rateSmoothed.setTargetValue (rateHz);
    depthSmoothed.setTargetValue (depth);
    widthSmoothed.setTargetValue (width);
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
        if (numSamples > (int) rateBuffer.size()) rateBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) depthBuffer.size()) depthBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) widthBuffer.size()) widthBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        delayBuffer[(size_t) sample] = delaySmoothed.getNextValue();
        rateBuffer[(size_t) sample] = rateSmoothed.getNextValue();
        depthBuffer[(size_t) sample] = depthSmoothed.getNextValue();
        widthBuffer[(size_t) sample] = widthSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
    }

    constexpr float maxModMs = 2.0f; // modulacion sutil, no queremos que suene a chorus

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float phaseInc = rateBuffer[(size_t) sample] / (float) sampleRate;
        const float smoothedDepth = depthBuffer[(size_t) sample];
        const float smoothedWidth = widthBuffer[(size_t) sample];
        const float smoothedMix = mixBuffer[(size_t) sample];
        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            // el canal derecho usa un delay base ligeramente distinto y el
            // LFO desfasado 90 grados: eso es lo que da el ancho estereo,
            // como una doble toma real donde las dos interpretaciones no
            // son identicas
            const float channelDelayOffset = (channel == 1) ? smoothedWidth * 6.0f : 0.0f;
            const float channelPhaseOffset = (channel == 1) ? 0.25f : 0.0f;

            float phase = lfoPhase + channelPhaseOffset;
            phase -= std::floor (phase);

            const float modMs = smoothedDepth * maxModMs * std::sin (juce::MathConstants<float>::twoPi * phase);
            const float delayMs = juce::jmax (1.0f, delayBuffer[(size_t) sample] + channelDelayOffset + modMs);
            const float delaySamples = delayMs / 1000.0f * (float) sampleRate;

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

            line[(size_t) wp] = input;
            wp = (wp + 1) % lineLength;

            channelData[sample] = input * (1.0f - smoothedMix) + wet * smoothedMix;
        }
    }
}

juce::AudioProcessorEditor* DoublerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (142, 120, 245));
}

void DoublerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DoublerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoublerAudioProcessor();
}
