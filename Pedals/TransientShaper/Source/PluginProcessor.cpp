#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout TransientShaperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ATTACK", 1 }, "Attack", -15.0f, 15.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SUSTAIN", 1 }, "Sustain", -15.0f, 15.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SENSITIVITY", 1 }, "Sensitivity", 0.5f, 5.0f, 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

TransientShaperAudioProcessor::TransientShaperAudioProcessor()
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

TransientShaperAudioProcessor::~TransientShaperAudioProcessor()
{
}

void TransientShaperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fastEnvelope = slowEnvelope = 0.0f;
    attackSmoothed.reset (sampleRate, 0.02);
    sustainSmoothed.reset (sampleRate, 0.02);
    sensitivitySmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    attackBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    sustainBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    sensitivityBuffer.assign ((size_t) samplesPerBlock, 2.0f);
    mixBuffer.assign ((size_t) samplesPerBlock, 1.0f);
    attackSmoothed.setCurrentAndTargetValue (0.0f);
    sustainSmoothed.setCurrentAndTargetValue (0.0f);
    sensitivitySmoothed.setCurrentAndTargetValue (2.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);

    fastAttackCoeff  = std::exp (-1.0f / (0.0005f * (float) sampleRate));
    fastReleaseCoeff = std::exp (-1.0f / (0.050f  * (float) sampleRate));
    slowAttackCoeff  = std::exp (-1.0f / (0.030f  * (float) sampleRate));
    slowReleaseCoeff = std::exp (-1.0f / (0.300f  * (float) sampleRate));
}

void TransientShaperAudioProcessor::releaseResources()
{
}

bool TransientShaperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void TransientShaperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float attackDb   = apvts.getRawParameterValue ("ATTACK")->load();
    const float sustainDb  = apvts.getRawParameterValue ("SUSTAIN")->load();
    const float sensitivity= apvts.getRawParameterValue ("SENSITIVITY")->load();
    const float mix        = apvts.getRawParameterValue ("MIX")->load();
    attackSmoothed.setTargetValue (attackDb);
    sustainSmoothed.setTargetValue (sustainDb);
    sensitivitySmoothed.setTargetValue (sensitivity);
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
        if (numSamples > (int) attackBuffer.size()) attackBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) sustainBuffer.size()) sustainBuffer.resize ((size_t) numSamples, 0.0f);
        if (numSamples > (int) sensitivityBuffer.size()) sensitivityBuffer.resize ((size_t) numSamples, 2.0f);
        if (numSamples > (int) mixBuffer.size()) mixBuffer.resize ((size_t) numSamples, 1.0f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        attackBuffer[(size_t) sample] = attackSmoothed.getNextValue();
        sustainBuffer[(size_t) sample] = sustainSmoothed.getNextValue();
        sensitivityBuffer[(size_t) sample] = sensitivitySmoothed.getNextValue();
        mixBuffer[(size_t) sample] = mixSmoothed.getNextValue();
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);

        const float fastCoeff = (peak > fastEnvelope) ? fastAttackCoeff : fastReleaseCoeff;
        fastEnvelope = peak + fastCoeff * (fastEnvelope - peak);

        const float slowCoeff = (peak > slowEnvelope) ? slowAttackCoeff : slowReleaseCoeff;
        slowEnvelope = peak + slowCoeff * (slowEnvelope - peak);

        // diferencia entre envolventes: positiva justo cuando pega un
        // transitorio (la rapida todavia no la alcanzo la lenta)
        const float detector = std::tanh ((fastEnvelope - slowEnvelope) * sensitivityBuffer[(size_t) sample] * 20.0f);
        const float attackWeight = 0.5f * (1.0f + detector); // 1 = transitorio, 0 = sustain

        const float gainDb = attackWeight * attackBuffer[(size_t) sample]
                   + (1.0f - attackWeight) * sustainBuffer[(size_t) sample];
        const float gain = juce::Decibels::decibelsToGain (gainDb);
        const float smoothedMix = mixBuffer[(size_t) sample];
        const float blendedGain = (1.0f - smoothedMix) + smoothedMix * gain;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            channelData[sample] *= blendedGain;
        }
    }
}

juce::AudioProcessorEditor* TransientShaperAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (100, 170, 250));
}

void TransientShaperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TransientShaperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TransientShaperAudioProcessor();
}
