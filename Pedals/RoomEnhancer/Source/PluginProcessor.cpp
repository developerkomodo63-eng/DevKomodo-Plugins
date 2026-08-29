#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout RoomEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Auto-locates and cuts persistent low-mid room resonance -- scans
    // three candidate bands and only touches whichever one is showing
    // genuinely constant (not just momentarily loud) energy.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "DEBOOM", 1 }, "Deboom", 0.0f, 10.0f, 5.0f));

    // Downward expansion keyed to how far below the recent peak the
    // current level has decayed -- digs into reverb/room tails and
    // hangover without a full dereverberation engine.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "TIGHTEN", 1 }, "Tighten", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

RoomEnhancerAudioProcessor::RoomEnhancerAudioProcessor()
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

RoomEnhancerAudioProcessor::~RoomEnhancerAudioProcessor()
{
}

void RoomEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.band1.prepare (spec);
        c.band2.prepare (spec);
        c.band3.prepare (spec);
        // Typical small/untreated-room mode range.
        c.band1.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 100.0f, 2.0f);
        c.band2.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 200.0f, 2.0f);
        c.band3.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 350.0f, 2.0f);
        c.reset();
    }

    // Deliberately slow -- this is the whole trick behind Deboom. Real
    // room resonance stays elevated over this window; passing vocal/
    // instrument content doesn't.
    slowCoeff = std::exp (-1.0f / (2.0f * (float) sampleRate));

    tailAttackCoeff  = std::exp (-1.0f / (0.005f * (float) sampleRate));
    tailReleaseCoeff = std::exp (-1.0f / (0.080f * (float) sampleRate));
    peakDecayCoeff   = std::exp (-1.0f / (0.400f * (float) sampleRate));
}

void RoomEnhancerAudioProcessor::releaseResources()
{
}

bool RoomEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void RoomEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float deboomNorm  = apvts.getRawParameterValue ("DEBOOM")->load()  / 10.0f;
    const float tightenNorm = apvts.getRawParameterValue ("TIGHTEN")->load() / 10.0f;
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    // A band sitting more than this far above the slow broadband
    // reference, sustained over the ~2s window, reads as a genuine mode.
    static constexpr float deboomBaseline = 1.6f;
    // Below this fraction of the recent peak, we're in "tail" territory.
    static constexpr float tailThreshold = 0.35f;
    static constexpr float maxTailReductionDb = 18.0f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        auto& c = channels[(size_t) channel];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = channelData[sample];
            const float absDry = std::abs (dry);

            // --- Deboom: scan three bands, track each against a slow
            // broadband reference, cut whichever is persistently hot.
            const float b1 = c.band1.processSample (dry);
            const float b2 = c.band2.processSample (dry);
            const float b3 = c.band3.processSample (dry);
            c.slowEnv1 = std::abs (b1) + slowCoeff * (c.slowEnv1 - std::abs (b1));
            c.slowEnv2 = std::abs (b2) + slowCoeff * (c.slowEnv2 - std::abs (b2));
            c.slowEnv3 = std::abs (b3) + slowCoeff * (c.slowEnv3 - std::abs (b3));
            c.slowBroadbandEnv = absDry + slowCoeff * (c.slowBroadbandEnv - absDry);
            const float reference = juce::jmax (c.slowBroadbandEnv, 0.004f);

            const float ratio1 = c.slowEnv1 / reference;
            const float ratio2 = c.slowEnv2 / reference;
            const float ratio3 = c.slowEnv3 / reference;
            const float reduction1 = deboomNorm * juce::jlimit (0.0f, 1.0f, (ratio1 - deboomBaseline) * 0.7f);
            const float reduction2 = deboomNorm * juce::jlimit (0.0f, 1.0f, (ratio2 - deboomBaseline) * 0.7f);
            const float reduction3 = deboomNorm * juce::jlimit (0.0f, 1.0f, (ratio3 - deboomBaseline) * 0.7f);
            const float boomCut = b1 * reduction1 + b2 * reduction2 + b3 * reduction3;
            const float deboomed = dry - boomCut;

            // --- Tighten: downward expansion of decay tails.
            const float tailCoeff = (absDry > c.tailEnv) ? tailAttackCoeff : tailReleaseCoeff;
            c.tailEnv = absDry + tailCoeff * (c.tailEnv - absDry);
            c.peakHold = juce::jmax (absDry, c.peakHold * peakDecayCoeff);
            const float relativeLevel = c.tailEnv / juce::jmax (c.peakHold, 0.004f);
            const float tailDeficit = juce::jmax (0.0f, 1.0f - relativeLevel / tailThreshold);
            const float reductionDb = tightenNorm * tailDeficit * maxTailReductionDb;
            const float tailGain = juce::Decibels::decibelsToGain (-reductionDb);

            // At Deboom=0 and Tighten=0 this reconstructs dry exactly.
            const float enhanced = deboomed * tailGain;
            channelData[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* RoomEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (140, 165, 190));
}

void RoomEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RoomEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomEnhancerAudioProcessor();
}
