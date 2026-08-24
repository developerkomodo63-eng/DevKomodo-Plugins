#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout DrumEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "CROSSOVER", 1 }, "Crossover",
        juce::NormalisableRange<float> { 200.0f, 2000.0f, 0.0f, 0.4f }, 600.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LOWDRIVE", 1 }, "Low Process", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "HIGHDRIVE", 1 }, "High Process", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ATTACK", 1 }, "Attack", -12.0f, 12.0f, 3.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUSTAIN", 1 }, "Sustain", -12.0f, 12.0f, -1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SNAP", 1 }, "Snap", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUB", 1 }, "Sub", 0.0f, 1.0f, 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PUNCH", 1 }, "Punch", 0.0f, 1.0f, 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

DrumEnhancerAudioProcessor::DrumEnhancerAudioProcessor()
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

DrumEnhancerAudioProcessor::~DrumEnhancerAudioProcessor()
{
}

void DrumEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fastEnvelope = slowEnvelope = 0.0f;
    subPhase = subEnvelope = previousTransient = 0.0f;
    fastAttackCoeff  = std::exp (-1.0f / (0.0005f * (float) sampleRate));
    fastReleaseCoeff = std::exp (-1.0f / (0.050f  * (float) sampleRate));
    slowAttackCoeff  = std::exp (-1.0f / (0.030f  * (float) sampleRate));
    slowReleaseCoeff = std::exp (-1.0f / (0.300f  * (float) sampleRate));
    attackSmoothed.reset (sampleRate, 0.02);
    sustainSmoothed.reset (sampleRate, 0.02);
    snapSmoothed.reset (sampleRate, 0.02);
    subSmoothed.reset (sampleRate, 0.02);
    punchSmoothed.reset (sampleRate, 0.02);
    attackSmoothed.setCurrentAndTargetValue (3.0f);
    sustainSmoothed.setCurrentAndTargetValue (-1.0f);
    snapSmoothed.setCurrentAndTargetValue (0.35f);
    subSmoothed.setCurrentAndTargetValue (0.25f);
    punchSmoothed.setCurrentAndTargetValue (0.45f);
    attackBuffer.assign ((size_t) samplesPerBlock, 3.0f);
    sustainBuffer.assign ((size_t) samplesPerBlock, -1.0f);
    snapBuffer.assign ((size_t) samplesPerBlock, 0.35f);
    subBuffer.assign ((size_t) samplesPerBlock, 0.25f);
    punchBuffer.assign ((size_t) samplesPerBlock, 0.45f);
    attackWeightBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    transientGainBuffer.assign ((size_t) samplesPerBlock, 1.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    subState.assign ((size_t) numChannels, 0.0f);
    splits.resize ((size_t) numChannels);
    for (auto& s : splits)
    {
        s.lowLP.prepare (spec);
        s.highHP.prepare (spec);
        s.lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        s.highHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        s.reset();
    }
}

void DrumEnhancerAudioProcessor::releaseResources()
{
}

bool DrumEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void DrumEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    const int processChannels = juce::jmin (totalNumInputChannels, totalNumOutputChannels);
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    const float crossover = apvts.getRawParameterValue ("CROSSOVER")->load();
    const float lowDrive  = apvts.getRawParameterValue ("LOWDRIVE")->load();
    const float highDrive = apvts.getRawParameterValue ("HIGHDRIVE")->load();
    const float attackDb  = apvts.getRawParameterValue ("ATTACK")->load();
    const float sustainDb = apvts.getRawParameterValue ("SUSTAIN")->load();
    const float snap       = apvts.getRawParameterValue ("SNAP")->load();
    const float sub        = apvts.getRawParameterValue ("SUB")->load();
    const float punch      = apvts.getRawParameterValue ("PUNCH")->load();
    const float mix       = apvts.getRawParameterValue ("MIX")->load();
    const float levelDb   = apvts.getRawParameterValue ("LEVEL")->load();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb);

    attackSmoothed.setTargetValue (attackDb);
    sustainSmoothed.setTargetValue (sustainDb);
    snapSmoothed.setTargetValue (snap);
    subSmoothed.setTargetValue (sub);
    punchSmoothed.setTargetValue (punch);

    if (numSamples > (int) attackBuffer.size()) attackBuffer.resize ((size_t) numSamples, attackDb);
    if (numSamples > (int) sustainBuffer.size()) sustainBuffer.resize ((size_t) numSamples, sustainDb);
    if (numSamples > (int) snapBuffer.size()) snapBuffer.resize ((size_t) numSamples, snap);
    if (numSamples > (int) subBuffer.size()) subBuffer.resize ((size_t) numSamples, sub);
    if (numSamples > (int) punchBuffer.size()) punchBuffer.resize ((size_t) numSamples, punch);
    if (numSamples > (int) attackWeightBuffer.size()) attackWeightBuffer.resize ((size_t) numSamples, 0.0f);
    if (numSamples > (int) transientGainBuffer.size()) transientGainBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        attackBuffer[(size_t) sample] = attackSmoothed.getNextValue();
        sustainBuffer[(size_t) sample] = sustainSmoothed.getNextValue();
        snapBuffer[(size_t) sample] = snapSmoothed.getNextValue();
        subBuffer[(size_t) sample] = subSmoothed.getNextValue();
        punchBuffer[(size_t) sample] = punchSmoothed.getNextValue();
    }

    for (auto& s : splits)
    {
        s.lowLP.setCutoffFrequency (crossover);
        s.highHP.setCutoffFrequency (crossover);
    }

    // cuanto mas alto el drive, mas ganancia interna antes de saturar
    // (rango pensado para armonicos sutiles, no para distorsion audible)
    const float lowGainStage  = 1.0f + lowDrive * 5.0f;
    const float highGainStage = 1.0f + highDrive * 5.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float detectorInput = 0.0f;
        for (int detectorChannel = 0; detectorChannel < processChannels; ++detectorChannel)
            detectorInput = juce::jmax (detectorInput,
                                        std::abs (buffer.getReadPointer (detectorChannel)[sample]));
        const float fastCoeff = (detectorInput > fastEnvelope) ? fastAttackCoeff : fastReleaseCoeff;
        fastEnvelope = detectorInput + fastCoeff * (fastEnvelope - detectorInput);
        const float slowCoeff = (detectorInput > slowEnvelope) ? slowAttackCoeff : slowReleaseCoeff;
        slowEnvelope = detectorInput + slowCoeff * (slowEnvelope - detectorInput);
        const float attackWeight = juce::jlimit (0.0f, 1.0f,
            (fastEnvelope - slowEnvelope) * 24.0f);
        if (attackWeight > 0.55f && attackWeight > previousTransient + 0.04f)
            subEnvelope = 1.0f;
        previousTransient = attackWeight;
        const float subDecay = std::exp (-1.0f / (0.115f * (float) getSampleRate()));
        subEnvelope *= subDecay;
        subPhase += juce::MathConstants<float>::twoPi * 55.0f / (float) getSampleRate();
        if (subPhase >= juce::MathConstants<float>::twoPi)
            subPhase -= juce::MathConstants<float>::twoPi;
        const float transientGainDb = attackWeight * attackBuffer[(size_t) sample]
                                    + (1.0f - attackWeight) * sustainBuffer[(size_t) sample];
        attackWeightBuffer[(size_t) sample] = attackWeight;
        transientGainBuffer[(size_t) sample] = juce::Decibels::decibelsToGain (transientGainDb);
    }

    for (int channel = 0; channel < processChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& s = splits[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];
            const float attackWeight = attackWeightBuffer[(size_t) sample];
            const float transientGain = transientGainBuffer[(size_t) sample];

            const float low  = s.lowLP.processSample (0, dry);
            const float high = s.highHP.processSample (0, dry);

            const float punchAmount = punchBuffer[(size_t) sample];
            const float subAmount = subBuffer[(size_t) sample];
            const float punchGain = 1.0f + attackWeight * punchAmount * 0.90f;
            const float snapGain = 1.0f + attackWeight * snapBuffer[(size_t) sample] * 0.85f;
            const float lowExcited = std::tanh (low * lowGainStage) / juce::jmax (lowGainStage, 1.0f);

            auto& subMemory = subState[(size_t) channel];
            subMemory += (std::abs (low) - subMemory) * 0.035f;
            const float lowBandEnvelope = juce::jlimit (0.0f, 1.0f, subMemory * 3.0f);
            const float subHarmonic = low * std::abs (low) * 1.8f * subAmount
                                    + std::sin (subPhase) * 0.32f * subAmount * subEnvelope * lowBandEnvelope;

            const float highExcited = std::tanh (high * highGainStage) / juce::jmax (highGainStage, 1.0f);

            const float enhanced = (lowExcited * lowDrive + low * (1.0f - lowDrive) + subHarmonic) * punchGain
                                  + (highExcited * highDrive + high * (1.0f - highDrive)) * snapGain;

            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * transientGain * outputGain;
        }
    }
}

juce::AudioProcessorEditor* DrumEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (255, 159, 109));
}

void DrumEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DrumEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumEnhancerAudioProcessor();
}
