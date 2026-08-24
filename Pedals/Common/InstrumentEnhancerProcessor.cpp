#include "InstrumentEnhancerProcessor.h"
#include "../DrumEnhancer/Source/DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout InstrumentEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "BODY", 1 }, "Body", 0.0f, 1.0f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "DETAIL", 1 }, "Detail", 0.0f, 1.0f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "HARMONICS", 1 }, "Harmonics", 0.0f, 1.0f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ATTACK", 1 }, "Attack", -12.0f, 12.0f, 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "SUSTAIN", 1 }, "Sustain", -12.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));
    return { params.begin(), params.end() };
}

InstrumentEnhancerAudioProcessor::InstrumentEnhancerAudioProcessor (InstrumentEnhancerType enhancerType)
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
 #if ! JucePlugin_IsSynth
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
 #endif
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ), type (enhancerType)
#else
    : type (enhancerType)
#endif
{
}

void InstrumentEnhancerAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    fastEnvelope = slowEnvelope = 0.0f;
    fastAttackCoeff  = std::exp (-1.0f / (0.0005f * (float) sampleRate));
    fastReleaseCoeff = std::exp (-1.0f / (0.050f  * (float) sampleRate));
    slowAttackCoeff  = std::exp (-1.0f / (0.030f  * (float) sampleRate));
    slowReleaseCoeff = std::exp (-1.0f / (0.300f  * (float) sampleRate));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    bands.resize ((size_t) channels);

    float bodyFrequency = 120.0f;
    float detailFrequency = 1000.0f;
    float airFrequency = 4500.0f;
    if (type == InstrumentEnhancerType::Bass)
    {
        bodyFrequency = 90.0f; detailFrequency = 850.0f; airFrequency = 3200.0f;
    }
    else if (type == InstrumentEnhancerType::Guitar)
    {
        bodyFrequency = 180.0f; detailFrequency = 1800.0f; airFrequency = 4200.0f;
    }
    else if (type == InstrumentEnhancerType::AcousticGuitar)
    {
        bodyFrequency = 140.0f; detailFrequency = 2800.0f; airFrequency = 7000.0f;
    }
    else
    {
        bodyFrequency = 260.0f; detailFrequency = 2100.0f; airFrequency = 6500.0f;
    }

    for (auto& channelBands : bands)
    {
        channelBands.body.prepare (spec);
        channelBands.body.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        channelBands.body.setCutoffFrequency (bodyFrequency);
        channelBands.detail.prepare (spec);
        channelBands.detail.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        channelBands.detail.setCutoffFrequency (detailFrequency);
        channelBands.air.prepare (spec);
        channelBands.air.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        channelBands.air.setCutoffFrequency (airFrequency);
        channelBands.reset();
    }

    bodySmoothed.reset (sampleRate, 0.02);
    detailSmoothed.reset (sampleRate, 0.02);
    harmonicsSmoothed.reset (sampleRate, 0.02);
    attackSmoothed.reset (sampleRate, 0.02);
    sustainSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    levelSmoothed.reset (sampleRate, 0.02);
    bodySmoothed.setCurrentAndTargetValue (0.35f);
    detailSmoothed.setCurrentAndTargetValue (0.35f);
    harmonicsSmoothed.setCurrentAndTargetValue (0.35f);
    attackSmoothed.setCurrentAndTargetValue (2.0f);
    sustainSmoothed.setCurrentAndTargetValue (0.0f);
    mixSmoothed.setCurrentAndTargetValue (0.7f);
    levelSmoothed.setCurrentAndTargetValue (1.0f);
    bodyBuffer.assign ((size_t) samplesPerBlock, 0.35f);
    detailBuffer.assign ((size_t) samplesPerBlock, 0.35f);
    harmonicsBuffer.assign ((size_t) samplesPerBlock, 0.35f);
    attackBuffer.assign ((size_t) samplesPerBlock, 2.0f);
    sustainBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 0.7f);
    levelBuffer.assign ((size_t) samplesPerBlock, 1.0f);
}

bool InstrumentEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == layouts.getMainInputChannelSet();
}

void InstrumentEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();
    const int processChannels = juce::jmin (inputChannels, outputChannels);
    const int numSamples = buffer.getNumSamples();
    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    const float body = apvts.getRawParameterValue ("BODY")->load();
    const float detail = apvts.getRawParameterValue ("DETAIL")->load();
    const float harmonics = apvts.getRawParameterValue ("HARMONICS")->load();
    const float attackDb = apvts.getRawParameterValue ("ATTACK")->load();
    const float sustainDb = apvts.getRawParameterValue ("SUSTAIN")->load();
    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const float level = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());
    bodySmoothed.setTargetValue (body); detailSmoothed.setTargetValue (detail);
    harmonicsSmoothed.setTargetValue (harmonics);
    attackSmoothed.setTargetValue (attackDb); sustainSmoothed.setTargetValue (sustainDb);
    mixSmoothed.setTargetValue (mix); levelSmoothed.setTargetValue (level);

    if (numSamples > (int) bodyBuffer.size()) bodyBuffer.resize ((size_t) numSamples, body);
    if (numSamples > (int) detailBuffer.size()) detailBuffer.resize ((size_t) numSamples, detail);
    if (numSamples > (int) harmonicsBuffer.size()) harmonicsBuffer.resize ((size_t) numSamples, harmonics);
    if (numSamples > (int) attackBuffer.size()) attackBuffer.resize ((size_t) numSamples, attackDb);
    if (numSamples > (int) sustainBuffer.size()) sustainBuffer.resize ((size_t) numSamples, sustainDb);
    if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, mix);
    if (numSamples > (int) levelBuffer.size()) levelBuffer.resize ((size_t) numSamples, level);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        bodyBuffer[(size_t) sample] = bodySmoothed.getNextValue();
        detailBuffer[(size_t) sample] = detailSmoothed.getNextValue();
        harmonicsBuffer[(size_t) sample] = harmonicsSmoothed.getNextValue();
        attackBuffer[(size_t) sample] = attackSmoothed.getNextValue();
        sustainBuffer[(size_t) sample] = sustainSmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
        levelBuffer[(size_t) sample] = levelSmoothed.getNextValue();
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float detector = 0.0f;
        for (int channel = 0; channel < processChannels; ++channel)
            detector = juce::jmax (detector, std::abs (buffer.getReadPointer (channel)[sample]));
        const float fastCoeff = detector > fastEnvelope ? fastAttackCoeff : fastReleaseCoeff;
        const float slowCoeff = detector > slowEnvelope ? slowAttackCoeff : slowReleaseCoeff;
        fastEnvelope = detector + fastCoeff * (fastEnvelope - detector);
        slowEnvelope = detector + slowCoeff * (slowEnvelope - detector);
        const float transient = juce::jlimit (0.0f, 1.0f, (fastEnvelope - slowEnvelope) * 24.0f);
        const float transientDb = transient * attackBuffer[(size_t) sample]
                                + (1.0f - transient) * sustainBuffer[(size_t) sample];
        for (int channel = 0; channel < processChannels; ++channel)
        {
            auto& channelBands = bands[(size_t) channel];
            float* data = buffer.getWritePointer (channel);
            const float dry = data[sample];
            const float bodyBand = channelBands.body.processSample (0, dry);
            const float detailBand = channelBands.detail.processSample (0, dry);
            const float airBand = channelBands.air.processSample (0, dry);
            const float harmonicAmount = harmonicsBuffer[(size_t) sample]
                                        * (type == InstrumentEnhancerType::Bass ? 1.35f : 1.0f);
            const float bodyHarmonics = std::tanh (bodyBand * (1.0f + bodyBuffer[(size_t) sample] * 4.0f
                                                                  + harmonicAmount * 2.0f));
            const float detailHarmonics = std::tanh (detailBand * (1.0f + detailBuffer[(size_t) sample] * 5.0f
                                                                      + harmonicAmount * 4.0f));
            const float airHarmonics = std::tanh (airBand * (1.0f + detailBuffer[(size_t) sample] * 3.0f
                                                                 + harmonicAmount * 2.0f));
            const float enhanced = dry
                + bodyHarmonics * bodyBuffer[(size_t) sample] * (type == InstrumentEnhancerType::Bass ? 0.85f : 0.55f)
                + detailHarmonics * detailBuffer[(size_t) sample] * (0.65f + harmonicAmount * 0.75f)
                + airHarmonics * detailBuffer[(size_t) sample] * (type == InstrumentEnhancerType::AcousticGuitar ? 0.75f : 0.35f);
            const float transientGain = juce::Decibels::decibelsToGain (transientDb);
            const float wet = mixBuffer[(size_t) sample];
            data[sample] = (dry * (1.0f - wet) + enhanced * wet) * transientGain * levelBuffer[(size_t) sample];
        }
    }
}

juce::AudioProcessorEditor* InstrumentEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (92, 184, 170));
}

void InstrumentEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void InstrumentEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
