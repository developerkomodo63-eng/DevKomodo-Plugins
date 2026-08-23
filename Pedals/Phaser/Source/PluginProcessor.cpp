#include "PluginProcessor.h"
#include "../../Common/TempoSync.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout PhaserAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 0.02f, 5.0f, 0.0f, 0.4f }, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "STAGES", 1 }, "Stages",
        juce::StringArray { "2", "4", "6", "8" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FEEDBACK", 1 }, "Feedback", -0.95f, 0.95f, 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

        DevKomodoTempoSync::addParameters (params, 4);

    return { params.begin(), params.end() };
}

PhaserAudioProcessor::PhaserAudioProcessor()
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

PhaserAudioProcessor::~PhaserAudioProcessor()
{
}

void PhaserAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    lfoPhase = 0.0f;
    rateSmoothed.reset (sampleRate, 0.02);
    depthSmoothed.reset (sampleRate, 0.02);
    feedbackSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    rateBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    depthBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    feedbackBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);

    for (auto& channelStages : stages)
        for (auto& stage : channelStages)
            stage = {};

    feedbackState.fill (0.0f);
}

void PhaserAudioProcessor::releaseResources()
{
}

bool PhaserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void PhaserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    float rateHz     = apvts.getRawParameterValue ("RATE")->load();
    {
        const bool tempoSynced = apvts.getRawParameterValue ("TEMPOSYNC")->load() > 0.5f;
        const int noteDivIndex = (int) apvts.getRawParameterValue ("NOTEDIV")->load();
        rateHz = DevKomodoTempoSync::resolveHz (*this, rateHz, tempoSynced, noteDivIndex, 0.02f, 5.0f);
    }
    const float depthBase  = apvts.getRawParameterValue ("DEPTH")->load();
    const float depth      = bassMode ? depthBase * 0.6f : depthBase;
    const int stagesChoice = (int) apvts.getRawParameterValue ("STAGES")->load();
    const float feedbackBase = apvts.getRawParameterValue ("FEEDBACK")->load();
    const float feedback   = bassMode ? feedbackBase * 0.45f : feedbackBase;
    const float mixBase     = apvts.getRawParameterValue ("MIX")->load();
    const float mix        = bassMode ? juce::jmin (mixBase, 0.6f) : mixBase;

    rateSmoothed.setTargetValue (rateHz);
    depthSmoothed.setTargetValue (depth);
    feedbackSmoothed.setTargetValue (feedback);
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
        if (numSamples > (int) rateBuffer.size()) rateBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) depthBuffer.size()) depthBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) feedbackBuffer.size()) feedbackBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        rateBuffer[(size_t) sample] = rateSmoothed.getNextValue();
        depthBuffer[(size_t) sample] = depthSmoothed.getNextValue();
        feedbackBuffer[(size_t) sample] = feedbackSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
    }

    const int numStages = 2 * (stagesChoice + 1); // choice 0..3 -> 2,4,6,8

    // barrido logaritmico entre ~200Hz y ~2kHz, controlado por el LFO y
    // escalado por Depth (a menos depth, el barrido es mas angosto)
    const float fMin = bassMode ? 450.0f : 200.0f;
    const float fMax = bassMode ? 2600.0f : 2000.0f;
    const float logMin = std::log (fMin);
    const float logMax = std::log (fMax);

    const int channelsToProcess = juce::jmin (totalNumInputChannels, (int) PhaserAudioProcessor::maxChannels);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float phaseInc = rateBuffer[(size_t) sample] / (float) sampleRate;
        const float smoothedDepth = depthBuffer[(size_t) sample];
        const float smoothedFeedback = feedbackBuffer[(size_t) sample];
        const float smoothedMix = mixBuffer[(size_t) sample];
        const float lfoUnipolar = 0.5f * (1.0f + std::sin (juce::MathConstants<float>::twoPi * lfoPhase));
        const float depthed = 0.5f - 0.5f * smoothedDepth + smoothedDepth * lfoUnipolar; // rango se achica con menos depth
        const float freqHz = std::exp (logMin + (logMax - logMin) * depthed);

        const float tanArg = juce::MathConstants<float>::pi * freqHz / (float) sampleRate;
        const float t = std::tan (tanArg);
        const float coeffA = (t - 1.0f) / (t + 1.0f);

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];

            const int stateChannel = juce::jmin (channel, channelsToProcess - 1);
            float chainInput = dry + feedbackState[(size_t) stateChannel] * smoothedFeedback;

            for (int s = 0; s < numStages; ++s)
                chainInput = processAllpass (stages[(size_t) stateChannel][(size_t) s], chainInput, coeffA);

            feedbackState[(size_t) stateChannel] = chainInput;

            channelData[sample] = dry * (1.0f - smoothedMix) + chainInput * smoothedMix;
        }
    }
}

juce::AudioProcessorEditor* PhaserAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (170, 120, 255));
}

void PhaserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PhaserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhaserAudioProcessor();
}
