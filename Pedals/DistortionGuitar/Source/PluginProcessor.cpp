#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout DistortionGuitarAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive",
        0.0f, 10.0f, 3.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SCOOP", 1 }, "Scoop", 0.0f, 1.0f, 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BIAS", 1 }, "Bias", 0.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone", 1000.0f, 8000.0f, 4000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 6.0f, -4.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "HQ", 1 }, "HQ", false));

    return { params.begin(), params.end() };
}

DistortionGuitarAudioProcessor::DistortionGuitarAudioProcessor()
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

DistortionGuitarAudioProcessor::~DistortionGuitarAudioProcessor()
{
}

void DistortionGuitarAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare(spec);
    hpFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency(100.0f);

    lpFilter.prepare(spec);
    lpFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    scoopFilters.resize ((size_t) spec.numChannels);
    for (auto& f : scoopFilters)
        f.reset();

    dcBlockerX1.assign((size_t) spec.numChannels, 0.0f);
    dcBlockerY1.assign((size_t) spec.numChannels, 0.0f);

    dryBuffer.setSize ((int) spec.numChannels, samplesPerBlock);
    driveSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    driveBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    outputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    driveSmoothed.setCurrentAndTargetValue (1.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
}

void DistortionGuitarAudioProcessor::releaseResources()
{
}

bool DistortionGuitarAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float DistortionGuitarAudioProcessor::processDistortionSample (float x, float bias) noexcept
{
    // Distortion is intentionally the hardest of the three drive families:
    // a steep symmetric knee, with Bias only adding a small optional
    // asymmetry. Unlike Overdrive this does not use a warm tanh curve, and
    // unlike Fuzz it does not fold the waveform back on itself.
    constexpr float hardness = 12.0f;
    const float biasOffset = bias * 0.12f;
    const float biased = x + biasOffset;
    const float ax = std::abs (biased);
    const float clipped = (ax < 0.75f)
        ? biased * (1.0f + 0.20f * ax * ax)
        : std::copysign (1.0f - std::exp (-hardness * (ax - 0.70f)), biased);

    const float restAx = std::abs (biasOffset);
    const float rest = (restAx < 0.75f)
        ? biasOffset * (1.0f + 0.20f * restAx * restAx)
        : std::copysign (1.0f - std::exp (-hardness * (restAx - 0.70f)), biasOffset);

    return clipped - rest;
}

void DistortionGuitarAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const bool bassMode = apvts.getRawParameterValue ("INSTRUMENT")->load() > 0.5f;

    const float driveKnob  = apvts.getRawParameterValue("DRIVE")->load();
    const float driveBase  = 1.0f + driveKnob * 7.9f;
    const float drive      = bassMode ? driveBase * 0.62f : driveBase;
    const float scoopBase  = apvts.getRawParameterValue("SCOOP")->load();
    const float scoop      = bassMode ? scoopBase * 0.08f : scoopBase * 1.10f;
    const float biasBase   = apvts.getRawParameterValue("BIAS")->load();
    const float bias       = bassMode ? biasBase * 0.7f : biasBase;
    const float toneCutoff = bassMode ? juce::jmax (1100.0f, apvts.getRawParameterValue("TONE")->load()) : juce::jmax (1800.0f, apvts.getRawParameterValue("TONE")->load());
    const float mix        = apvts.getRawParameterValue("MIX")->load();
    const float levelDb    = apvts.getRawParameterValue("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain(levelDb);
    const bool hq = apvts.getRawParameterValue ("HQ")->load() > 0.5f;

    driveSmoothed.setTargetValue (drive);
    mixSmoothed.setTargetValue (mix);
    outputGainSmoothed.setTargetValue (outputGain);

    lpFilter.setCutoffFrequency(toneCutoff);

    // scoop de medios: peak filter con ganancia negativa alrededor de 650Hz.
    // Los coeficientes se comparten (son los mismos para L y R), pero el
    // estado interno de cada filtro de canal es independiente.
    auto scoopCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        currentSampleRate, 650.0f, 0.8f, juce::Decibels::decibelsToGain (-9.0f * scoop));
    for (auto& f : scoopFilters)
        *f.coefficients = scoopCoeffs;

    const int numSamples = buffer.getNumSamples();
    // jassert-only bounds checks are compiled out entirely in Release
    // builds, so they gave zero real protection: if a host/exporter uses
    // a block size larger than what prepareToPlay() originally sized
    // these buffers for (this happens with some DAWs' offline bounce/
    // export, which can use a different block size than realtime
    // playback), the per-sample smoothing loop below would write past
    // the end of these vectors -- a real heap buffer overflow, not just
    // a debug-mode warning. Actually growing the buffers here fixes it
    // for any block size the host throws at us.
        if (numSamples > (int) driveBuffer.size()) driveBuffer.resize ((size_t) numSamples, 1.0f);
        if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, 1.0f);
        if (numSamples > (int) outputGainBuffer.size()) outputGainBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        driveBuffer[(size_t) sample] = driveSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        outputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    hpFilter.setCutoffFrequency (bassMode ? 25.0f : 100.0f);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = hpFilter.processSample (channel, channelData[sample]);
    }

    juce::dsp::AudioBlock<float> audioBlock (buffer);
    auto processingBlock = audioBlock;
    if (hq)
        processingBlock = oversampling.processSamplesUp (processingBlock);
    const int processingSamples = (int) processingBlock.getNumSamples();

    // HQ applies oversampling only around the nonlinear stage; filters and
    // dry/wet mixing remain at the host sample rate.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* data = processingBlock.getChannelPointer ((size_t) channel);
        for (int sample = 0; sample < processingSamples; ++sample)
        {
            const int sourceSample = juce::jmin (numSamples - 1, sample / (hq ? 4 : 1));
            data[sample] = processDistortionSample (data[sample] * driveBuffer[(size_t) sourceSample], bias);
        }
    }

    if (hq)
        oversampling.processSamplesDown (audioBlock);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        const float* dry = dryBuffer.getReadPointer (channel);
        float& x1 = dcBlockerX1[(size_t) channel];
        float& y1 = dcBlockerY1[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float scooped = scoopFilters[(size_t) channel].processSample (channelData[sample]);
            float toneSample = lpFilter.processSample (channel, scooped);

            const float x0 = toneSample;
            const float y0 = x0 - x1 + dcBlockerR * y1;
            x1 = x0;
            y1 = y0;

            const float smoothedMix = mixBuffer[(size_t) sample];
            channelData[sample] = (dry[sample] * (1.0f - smoothedMix) + y0 * smoothedMix)
                                * outputGainBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* DistortionGuitarAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 87, 51));
}

void DistortionGuitarAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DistortionGuitarAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionGuitarAudioProcessor();
}
