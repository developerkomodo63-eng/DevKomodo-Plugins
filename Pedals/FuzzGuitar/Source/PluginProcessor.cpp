#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout FuzzGuitarAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "FUZZ", 1 }, "Fuzz", 0.0f, 10.0f, 7.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 10.0f, 2.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BIAS", 1 }, "Bias", -1.0f, 1.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone", 800.0f, 8000.0f, 3500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 6.0f, -6.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "HQ", 1 }, "HQ", false));

    return { params.begin(), params.end() };
}

FuzzGuitarAudioProcessor::FuzzGuitarAudioProcessor()
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

FuzzGuitarAudioProcessor::~FuzzGuitarAudioProcessor()
{
}

void FuzzGuitarAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare(spec);
    hpFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency(100.0f);

    lpFilter.prepare(spec);
    lpFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

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

void FuzzGuitarAudioProcessor::releaseResources()
{
}

bool FuzzGuitarAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float FuzzGuitarAudioProcessor::processFuzzSample (float x, float hardness, float bias) noexcept
{
    // Fuzz deliberately folds the waveform instead of behaving like a
    // conventional soft clipper. This creates the dense odd/even harmonic
    // texture that separates it clearly from Distortion and Saturator.
    const float biased = x + bias * 0.26f;
    const float pre = std::tanh (biased * (1.0f + hardness * 2.2f));
    const float p2 = pre * pre;
    const float folded = pre * (1.0f - 1.55f * p2 + 0.55f * p2 * p2);

    const float diodeK = 2.0f + hardness * 9.0f;
    const float sign = biased >= 0.0f ? 1.0f : -1.0f;
    const float diode = sign * (1.0f - std::exp (-diodeK * std::abs (biased)));
    const float shaped = folded * (0.72f + 0.18f * hardness) + diode * (0.28f - 0.18f * hardness);

    const float restBias = bias * 0.26f;
    const float restPre = std::tanh (restBias * (1.0f + hardness * 2.2f));
    const float restP2 = restPre * restPre;
    const float restFolded = restPre * (1.0f - 1.55f * restP2 + 0.55f * restP2 * restP2);
    const float restSign = restBias >= 0.0f ? 1.0f : -1.0f;
    const float restDiode = restSign * (1.0f - std::exp (-diodeK * std::abs (restBias)));
    const float rest = restFolded * (0.72f + 0.18f * hardness) + restDiode * (0.28f - 0.18f * hardness);

    return juce::jlimit (-1.0f, 1.0f, shaped - rest);
}

void FuzzGuitarAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float fuzzAmountKnob = apvts.getRawParameterValue("FUZZ")->load();
    const float fuzzAmountBase = fuzzAmountKnob * 0.10f;
    const float fuzzAmount = bassMode ? fuzzAmountBase * 0.68f : fuzzAmountBase * 1.05f;
    const float driveKnob   = apvts.getRawParameterValue("DRIVE")->load();
    const float driveBase   = 1.0f + driveKnob * 9.9f;
    const float drive       = bassMode ? driveBase * 0.58f : driveBase;
    const float bias        = apvts.getRawParameterValue("BIAS")->load();
    const float toneCutoff  = bassMode ? juce::jmax (1000.0f, apvts.getRawParameterValue("TONE")->load()) : juce::jmax (1800.0f, apvts.getRawParameterValue("TONE")->load());
    const float mix         = apvts.getRawParameterValue("MIX")->load();
    const float levelDb     = apvts.getRawParameterValue("LEVEL")->load();
    const float outputGain  = juce::Decibels::decibelsToGain(levelDb);
    const bool hq = apvts.getRawParameterValue ("HQ")->load() > 0.5f;

    driveSmoothed.setTargetValue (drive);
    mixSmoothed.setTargetValue (mix);
    outputGainSmoothed.setTargetValue (outputGain);

    lpFilter.setCutoffFrequency(toneCutoff);

    const int numSamples = buffer.getNumSamples();
    jassert (numSamples <= (int) driveBuffer.size());
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

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* data = processingBlock.getChannelPointer ((size_t) channel);
        for (int sample = 0; sample < processingSamples; ++sample)
        {
            const int sourceSample = juce::jmin (numSamples - 1, sample / (hq ? 4 : 1));
            data[sample] = processFuzzSample (data[sample] * driveBuffer[(size_t) sourceSample], fuzzAmount, bias);
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
            const float toneSample = lpFilter.processSample (channel, channelData[sample]);

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

juce::AudioProcessorEditor* FuzzGuitarAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (214, 69, 65));
}

void FuzzGuitarAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FuzzGuitarAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FuzzGuitarAudioProcessor();
}
