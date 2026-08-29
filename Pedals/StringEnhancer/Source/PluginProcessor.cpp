#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout StringEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Sympathetic-resonance style body bloom -- fixed resonant peaks,
    // not tied to whatever note is being played (matches how real
    // instrument body resonance actually works).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "BLOOM", 1 }, "Bloom", 0.0f, 10.0f, 4.0f));

    // Adaptive taming of the harsh 2-4kHz register that massed/aggressive
    // bowed strings can shriek in -- only cuts when that band is actually
    // hot, not a static EQ dip.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SMOOTH", 1 }, "Smooth", 0.0f, 10.0f, 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

StringEnhancerAudioProcessor::StringEnhancerAudioProcessor()
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

StringEnhancerAudioProcessor::~StringEnhancerAudioProcessor()
{
}

void StringEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.resonator1.prepare (spec);
        c.resonator2.prepare (spec);
        c.resonator3.prepare (spec);
        c.shriekBand.prepare (spec);
        // Slightly detuned from a pure harmonic series (210/415/830
        // rather than 220/440/880) so the bloom doesn't reinforce
        // whatever note is being played and sound artificially "tuned" --
        // real body resonances aren't perfect harmonics of the note
        // either. High Q gives each filter its own short ringing tail
        // when struck, which is what reads as "bloom".
        c.resonator1.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 210.0f, 14.0f);
        c.resonator2.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 415.0f, 11.0f);
        c.resonator3.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 830.0f, 9.0f);
        c.shriekBand.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 3000.0f, 1.0f);
        c.reset();
    }

    shriekEnvCoeff = std::exp (-1.0f / (0.015f * (float) sampleRate));
}

void StringEnhancerAudioProcessor::releaseResources()
{
}

bool StringEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void StringEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float bloomNorm  = apvts.getRawParameterValue ("BLOOM")->load()  / 10.0f;
    const float smoothNorm = apvts.getRawParameterValue ("SMOOTH")->load() / 10.0f;
    const float mix        = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    static constexpr float shriekThreshold = 0.05f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float r1 = c.resonator1.processSample (dry);
            const float r2 = c.resonator2.processSample (dry);
            const float r3 = c.resonator3.processSample (dry);
            const float bloomOut = (r1 * 0.5f + r2 * 0.35f + r3 * 0.25f) * bloomNorm * 1.4f;

            const float shriek = c.shriekBand.processSample (dry);
            const float absShriek = std::abs (shriek);
            c.shriekEnv = absShriek + shriekEnvCoeff * (c.shriekEnv - absShriek);
            const float excess = juce::jmax (0.0f, c.shriekEnv - shriekThreshold);
            const float reduction = smoothNorm * juce::jmin (1.0f, excess * 8.0f);
            const float shriekCut = shriek * reduction;

            // At Bloom=0 and Smooth=0 this reconstructs dry exactly.
            const float enhanced = dry - shriekCut + bloomOut;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* StringEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (168, 120, 220));
}

void StringEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void StringEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StringEnhancerAudioProcessor();
}
