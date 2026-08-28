#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout AcousticGuitarEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Spectral-tilt de-boxing: compares the ~250Hz body-resonance band
    // against a neighboring ~500Hz reference band and cuts based on how
    // tilted the balance is between them, rather than comparing one band
    // to the total signal energy.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEBOX", 1 }, "Debox", 0.0f, 10.0f, 5.0f));

    // Crest-factor-gated sparkle: reacts to how "spiky" the signal is
    // right now (peak vs. recent RMS), which is what a pick/strum attack
    // actually looks like mathematically, rather than timing an envelope.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SPARKLE", 1 }, "Sparkle", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

AcousticGuitarEnhancerAudioProcessor::AcousticGuitarEnhancerAudioProcessor()
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

AcousticGuitarEnhancerAudioProcessor::~AcousticGuitarEnhancerAudioProcessor()
{
}

void AcousticGuitarEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.boxBand.prepare (spec);
        c.referenceBand.prepare (spec);
        c.sparkleHP.prepare (spec);
        c.boxBand.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 250.0f, 1.2f);
        c.referenceBand.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 500.0f, 1.0f);
        c.sparkleHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.sparkleHP.setCutoffFrequency (7000.0f);
        c.reset();
    }

    const auto sr = (float) sampleRate;
    envCoeff       = std::exp (-1.0f / (0.020f * sr));
    peakDecayCoeff = std::exp (-1.0f / (0.050f * sr));
    rmsCoeff       = std::exp (-1.0f / (0.030f * sr));
}

void AcousticGuitarEnhancerAudioProcessor::releaseResources()
{
}

bool AcousticGuitarEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AcousticGuitarEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float deboxNorm   = apvts.getRawParameterValue ("DEBOX")->load()   / 10.0f;
    const float sparkleNorm = apvts.getRawParameterValue ("SPARKLE")->load() / 10.0f;
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    // A normal acoustic guitar's box band naturally runs a bit hotter
    // than the reference band above it; only tilt beyond this baseline
    // ratio counts as "too boxy".
    static constexpr float baselineTilt = 1.3f;
    // Below this crest factor, treat the signal as sustained/steady (a
    // sine-like tone sits around 1.4-2); above it, increasingly "spiky".
    static constexpr float crestFloor = 2.0f;
    static constexpr float crestSpan  = 6.0f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];

            const float boxSample = c.boxBand.processSample (dry);
            const float refSample = c.referenceBand.processSample (dry);
            const float absBox = std::abs (boxSample);
            const float absRef = std::abs (refSample);
            c.boxEnv = absBox + envCoeff * (c.boxEnv - absBox);
            c.referenceEnv = absRef + envCoeff * (c.referenceEnv - absRef);

            const float tilt = c.boxEnv / juce::jmax (c.referenceEnv, 0.004f);
            const float excess = juce::jmax (0.0f, tilt - baselineTilt);
            const float reduction = deboxNorm * juce::jmin (1.0f, excess * 0.8f);
            const float boxCut = boxSample * reduction;

            // Crest factor: instantaneous peak (with decay) versus a
            // short-window RMS. A sharp strum attack spikes the peak
            // well above the recent RMS; a sustained note doesn't.
            const float absDry = std::abs (dry);
            c.peakHold = juce::jmax (absDry, c.peakHold * peakDecayCoeff);
            const float squared = absDry * absDry;
            c.rmsSquaredEnv = squared + rmsCoeff * (c.rmsSquaredEnv - squared);
            const float rms = std::sqrt (juce::jmax (c.rmsSquaredEnv, 1.0e-9f));
            const float crestFactor = c.peakHold / juce::jmax (rms, 0.004f);
            const float sparkleAmount = juce::jlimit (0.0f, 1.0f, (crestFactor - crestFloor) / crestSpan);

            const float sparkleBand = c.sparkleHP.processSample (0, dry);
            const float sparkleGain = sparkleNorm * sparkleAmount * 2.2f;

            // At Debox=0 and Sparkle=0 this reconstructs dry exactly.
            const float enhanced = dry - boxCut + sparkleBand * sparkleGain;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* AcousticGuitarEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (222, 190, 130));
}

void AcousticGuitarEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AcousticGuitarEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcousticGuitarEnhancerAudioProcessor();
}
