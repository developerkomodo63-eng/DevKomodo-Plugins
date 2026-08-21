#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout CompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "THRESHOLD", 1 }, "Threshold", -40.0f, 0.0f, -18.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RATIO", 1 }, "Ratio",
        juce::NormalisableRange<float> { 1.0f, 20.0f, 0.0f, 0.4f }, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float> { 0.1f, 100.0f, 0.0f, 0.4f }, 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "RELEASE", 1 }, "Release",
        juce::NormalisableRange<float> { 10.0f, 1000.0f, 0.0f, 0.4f }, 120.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MAKEUP", 1 }, "Makeup Gain", 0.0f, 24.0f, 6.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    // Absorbed from the standalone Limiter plugin: instead of a separate
    // pedal with its own (differently-behaved) algorithm, Limiter mode here
    // forces a near-instant attack and a very high effective ratio -- a
    // brickwall-style ceiling using the same clean gain-computer as
    // Compressor mode, rather than switching to a different distortion-y
    // tanh-based algorithm. RATIO/ATTACK stay visible but are ignored while
    // Limiter mode is active, since a limiter's whole point is that those
    // aren't user-tunable.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "MODE", 1 }, "Mode",
        juce::StringArray { "Compressor", "Limiter" }, 0));

    return { params.begin(), params.end() };
}

CompressorAudioProcessor::CompressorAudioProcessor()
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

CompressorAudioProcessor::~CompressorAudioProcessor()
{
}

void CompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    envelopeDb = -100.0f;
    // los coeficientes reales dependen del Attack/Release del usuario y se
    // recalculan en processBlock; esto es solo un valor inicial razonable
    attackCoeff  = std::exp (-1.0f / (0.008f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.120f * (float) sampleRate));
    makeupGainSmoothed.reset (sampleRate, 0.02);
    makeupGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (6.0f));
    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
}

void CompressorAudioProcessor::releaseResources()
{
}

bool CompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float thresholdDb = apvts.getRawParameterValue ("THRESHOLD")->load();
    const float ratio       = apvts.getRawParameterValue ("RATIO")->load();
    const float attackMs    = apvts.getRawParameterValue ("ATTACK")->load();
    const float releaseMs   = apvts.getRawParameterValue ("RELEASE")->load();
    const float makeupDb    = apvts.getRawParameterValue ("MAKEUP")->load();
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    const bool limiterMode  = (int) apvts.getRawParameterValue ("MODE")->load() > 0;

    // Limiter mode: near-instant attack, effectively-infinite ratio. This
    // reuses the exact same envelope-follower/gain-computer code path below
    // (just with different constants), rather than a second algorithm.
    const float effectiveAttackMs = limiterMode ? 0.5f : attackMs;
    const float effectiveRatio    = limiterMode ? 100.0f : ratio;

    makeupGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (makeupDb));
    mixSmoothed.setTargetValue (mix);

    const float sr = (float) getSampleRate();
    attackCoeff  = std::exp (-1.0f / (juce::jmax (effectiveAttackMs, 0.1f) / 1000.0f * sr));
    releaseCoeff = std::exp (-1.0f / (juce::jmax (releaseMs, 1.0f) / 1000.0f * sr));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // detectamos nivel de pico (canal 0), y linkeamos todos los canales
        // a la misma reduccion de ganancia para que no se muevan las fases
        const float peak = std::abs (buffer.getReadPointer (0)[sample]);
        const float peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);

        const float envCoeff = (peakDb > envelopeDb) ? attackCoeff : releaseCoeff;
        envelopeDb = peakDb + envCoeff * (envelopeDb - peakDb);

        float gainReductionDb = 0.0f;
        if (envelopeDb > thresholdDb)
        {
            const float overDb = envelopeDb - thresholdDb;
            gainReductionDb = overDb - overDb / effectiveRatio;
        }

        const float totalGain = makeupGainSmoothed.getNextValue()
                      * juce::Decibels::decibelsToGain (-gainReductionDb);
        const float smoothedMix = mixSmoothed.getNextValue();

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* channelData = buffer.getWritePointer (channel);
            const float dry = channelData[sample];
            const float compressed = dry * totalGain;
            channelData[sample] = dry * (1.0f - smoothedMix) + compressed * smoothedMix;
        }
    }
}

juce::AudioProcessorEditor* CompressorAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (66, 158, 255));
}

void CompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressorAudioProcessor();
}
