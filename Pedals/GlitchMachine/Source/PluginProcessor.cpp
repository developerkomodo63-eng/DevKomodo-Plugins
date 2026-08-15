#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout GlitchMachineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATE", 1 }, "Rate",
        juce::NormalisableRange<float> { 25.0f, 500.0f, 0.0f, 0.4f }, 150.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CHAOS", 1 }, "Chaos", 0.0f, 1.0f, 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEPTH", 1 }, "Depth", 0.0f, 1.0f, 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DRIFT", 1 }, "Drift", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.82f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -18.0f, 12.0f, -2.0f));

    return { params.begin(), params.end() };
}

GlitchMachineAudioProcessor::GlitchMachineAudioProcessor()
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

GlitchMachineAudioProcessor::~GlitchMachineAudioProcessor()
{
}

void GlitchMachineAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;
    rng.setSeedRandomly();

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    const int bufferLength = (int) (0.7 * sampleRate) + 8;

    channels.assign ((size_t) numChannels, ChannelState());
    for (auto& c : channels)
    {
        c.captureBuffer.assign ((size_t) bufferLength, 0.0f);
        c.writePos = 0;
        c.slotSamplesRemaining = 0;
        c.currentMode = SlotMode::Passthrough;
    }
}

void GlitchMachineAudioProcessor::releaseResources()
{
}

bool GlitchMachineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void GlitchMachineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float rateMs  = apvts.getRawParameterValue ("RATE")->load();
    const float chaos   = apvts.getRawParameterValue ("CHAOS")->load();
    const float depth   = apvts.getRawParameterValue ("DEPTH")->load();
    const float drift   = apvts.getRawParameterValue ("DRIFT")->load();
    const float mix     = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    const int baseSlotSamples = juce::jmax (16, (int) (rateMs / 1000.0f * (float) sampleRate));

    constexpr float pitchOptions[5] = { 1.0f, 0.5f, 2.0f, 0.75f, 1.5f };

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];
        const int bufferLength = (int) c.captureBuffer.size();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float input = channelData[sample];
            c.captureBuffer[(size_t) c.writePos] = input;

            if (c.slotSamplesRemaining <= 0)
            {
                const float jitter = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * (chaos * 0.7f + depth * 0.35f);
                c.slotSamplesRemaining = juce::jmax (10, (int) ((float) baseSlotSamples * jitter));

                const float roll = rng.nextFloat();
                const float glitchChance = juce::jlimit (0.0f, 1.0f, chaos * 0.7f + depth * 0.35f);
                if (roll < glitchChance * 0.32f)
                {
                    c.currentMode = SlotMode::Silence;
                }
                else if (roll < glitchChance * 0.78f)
                {
                    c.currentMode = SlotMode::Glitch;

                    c.segmentLength = juce::jmax (8, (int) ((float) c.slotSamplesRemaining
                                                   * (0.18f + rng.nextFloat() * 0.72f)));
                    c.segmentLength = juce::jmin (c.segmentLength, bufferLength - 8);

                    c.segmentStart = c.writePos - c.segmentLength;
                    while (c.segmentStart < 0)
                        c.segmentStart += bufferLength;

                    c.direction = (rng.nextFloat() < 0.6f) ? 1.0f : -1.0f;

                    const int pitchIndex = (rng.nextFloat() < chaos * 0.55f)
                        ? 1 + (int) (rng.nextFloat() * 4.0f)
                        : 0;
                    const float pitchBias = 1.0f + (drift * 1.3f) * (rng.nextFloat() * 2.0f - 1.0f);
                    c.pitchMult = juce::jlimit (0.45f, 2.2f, pitchOptions[juce::jlimit (0, 4, pitchIndex)] * pitchBias);

                    c.progress = 0.0f;
                }
                else
                {
                    c.currentMode = SlotMode::Passthrough;
                }
            }
            --c.slotSamplesRemaining;

            float wet = input;
            if (c.currentMode == SlotMode::Silence)
            {
                wet = 0.0f;
            }
            else if (c.currentMode == SlotMode::Glitch)
            {
                c.progress += c.pitchMult * (1.0f + drift * 0.8f);
                float localPos = std::fmod (c.progress, (float) c.segmentLength);
                if (c.direction < 0.0f)
                    localPos = (float) c.segmentLength - 1.0f - localPos;

                const int readIndex = (c.segmentStart + (int) localPos) % bufferLength;
                wet = c.captureBuffer[(size_t) readIndex];

                const float bitDepth = 1.0f + (1.0f - depth) * 7.5f;
                wet = std::round (wet * bitDepth) / bitDepth;
                wet = juce::jlimit (-1.0f, 1.0f, wet * (0.9f + depth * 1.6f));
            }

            c.writePos = (c.writePos + 1) % bufferLength;

            const float body = input * (1.0f - mix) + wet * mix;
            const float driven = std::tanh (body * (1.2f + depth * 1.6f));
            channelData[sample] = driven * outputGain;
        }
    }
}

juce::AudioProcessorEditor* GlitchMachineAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name,
                                        juce::Colour::fromRGB (255, 110, 165));
}

void GlitchMachineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GlitchMachineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlitchMachineAudioProcessor();
}
