#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout OverdriveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "INSTRUMENT", 1 }, "Instrument",
        juce::StringArray { "Guitar", "Bass" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 10.0f, 2.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TONE", 1 }, "Tone", 1000.0f, 8000.0f, 4500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CHARACTER", 1 }, "Character", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 6.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "HQ", 1 }, "HQ", false));

    return { params.begin(), params.end() };
}

OverdriveAudioProcessor::OverdriveAudioProcessor()
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

OverdriveAudioProcessor::~OverdriveAudioProcessor()
{
}

void OverdriveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    hpFilter.prepare(spec);
    hpFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    hpFilter.setCutoffFrequency(25.0f);

    lpFilter.prepare(spec);
    lpFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    midPushFilter.reset();
    midPushFilter.prepare(spec);

    dcBlockerX1.assign((size_t) spec.numChannels, 0.0f);
    dcBlockerY1.assign((size_t) spec.numChannels, 0.0f);
    smoothedDriveBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    smoothedOutputGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    driveSmoothed.reset (sampleRate, 0.02);
    driveSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
}

void OverdriveAudioProcessor::releaseResources()
{
}

bool OverdriveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

float OverdriveAudioProcessor::processSaturationSample (float x, float character) noexcept
{
    // Character controla la asimetria: en 0 casi simetrico (mas tenso,
    // mas parecido a un op-amp), en 1 bien sesgado (mas parecido a una
    // sola valvula, mas 2do armonico). 0.12 era el valor fijo anterior;
    // ahora es el techo del rango, no un numero cerrado.
    const float bias = character * 0.24f;
    const float biased = x + bias;
    const float cubic = biased - (biased * biased * biased) * 0.14f;
    const float shaped = std::tanh(cubic);

    return shaped - std::tanh(bias - (bias * bias * bias) * 0.14f);
}

void OverdriveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float driveKnob = apvts.getRawParameterValue("DRIVE")->load();
    const float driveBase = 1.0f + driveKnob * 3.9f;
    const float drive = bassMode ? driveBase * 0.72f : driveBase;
    const float toneCutoff = bassMode ? juce::jmax (1000.0f, apvts.getRawParameterValue("TONE")->load()) : juce::jmax (1800.0f, apvts.getRawParameterValue("TONE")->load());
    const float character = apvts.getRawParameterValue("CHARACTER")->load();
    const float levelDb = apvts.getRawParameterValue("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain(levelDb);
    const bool hq = apvts.getRawParameterValue ("HQ")->load() > 0.5f;

    const float midFrequency = bassMode ? 220.0f : 850.0f;
    const float midGainDb = bassMode ? 2.5f : 4.5f + character * 1.5f;
    *midPushFilter.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
        getSampleRate(), midFrequency, 0.75f,
        juce::Decibels::decibelsToGain (midGainDb));

    driveSmoothed.setTargetValue (drive);
    outputGainSmoothed.setTargetValue (outputGain);

    lpFilter.setCutoffFrequency(toneCutoff);

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
        if (numSamples > (int) smoothedDriveBuffer.size()) smoothedDriveBuffer.resize ((size_t) numSamples, 1.0f);
        if (numSamples > (int) smoothedOutputGainBuffer.size()) smoothedOutputGainBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        smoothedDriveBuffer[(size_t) sample] = driveSmoothed.getNextValue();
        smoothedOutputGainBuffer[(size_t) sample] = outputGainSmoothed.getNextValue();
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] = hpFilter.processSample (channel, channelData[sample]);
            channelData[sample] = midPushFilter.processSample (channel, channelData[sample]);
        }
    }

    juce::dsp::AudioBlock<float> audioBlock (buffer);
    auto processingBlock = audioBlock;
    if (hq)
        processingBlock = oversampling.processSamplesUp (processingBlock);
    const int processingSamples = (int) processingBlock.getNumSamples();
    const int oversamplingFactor = hq ? 4 : 1;

    // Native-rate nonlinear stage keeps CPU and latency extremely low.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* data = processingBlock.getChannelPointer ((size_t) channel);
        for (int sample = 0; sample < processingSamples; ++sample)
        {
            const int sourceSample = juce::jmin (numSamples - 1, sample / oversamplingFactor);
            data[sample] = processSaturationSample (data[sample] * smoothedDriveBuffer[(size_t) sourceSample], character);
        }
    }

    if (hq)
        oversampling.processSamplesDown (audioBlock);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        float& x1 = dcBlockerX1[(size_t) channel];
        float& y1 = dcBlockerY1[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float toneSample = lpFilter.processSample (channel, channelData[sample]);

            // DC blocker de un polo: y[n] = x[n] - x[n-1] + R*y[n-1]
            const float x0 = toneSample;
            const float y0 = x0 - x1 + dcBlockerR * y1;
            x1 = x0;
            y1 = y0;

            channelData[sample] = y0 * smoothedOutputGainBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* OverdriveAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 140, 66));
}

void OverdriveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void OverdriveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OverdriveAudioProcessor();
}
