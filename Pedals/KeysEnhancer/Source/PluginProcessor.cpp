#include "PluginProcessor.h"
#include "DevKomodoUI.h"

juce::AudioProcessorValueTreeState::ParameterLayout KeysEnhancerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Timed bloom: silent right at the attack, blooms in over the
    // following ~250ms, holds, then fades -- driven by elapsed time since
    // the last detected onset, not by comparing signal levels.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SHIMMER", 1 }, "Shimmer", 0.0f, 10.0f, 4.0f));

    // Adaptive stereo width: widens during sustain, narrows back toward
    // mono right on a new note attack -- a static M-S widener smears
    // transients, this keeps onsets centered/punchy.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "WIDTH", 1 }, "Width", 0.0f, 10.0f, 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.75f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LEVEL", 1 }, "Level", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

KeysEnhancerAudioProcessor::KeysEnhancerAudioProcessor()
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

KeysEnhancerAudioProcessor::~KeysEnhancerAudioProcessor()
{
}

void KeysEnhancerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    channels.resize ((size_t) numChannels);
    for (auto& c : channels)
    {
        c.shimmerHP.prepare (spec);
        c.shimmerHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        c.shimmerHP.setCutoffFrequency (5000.0f);
        c.reset();
    }

    const auto sr = (float) sampleRate;
    fastAttackCoeff  = std::exp (-1.0f / (0.001f * sr));
    fastReleaseCoeff = std::exp (-1.0f / (0.040f * sr));
    slowAttackCoeff  = std::exp (-1.0f / (0.080f * sr));
    slowReleaseCoeff = std::exp (-1.0f / (0.400f * sr));
    widthSmoothCoeff = 1.0f - std::exp (-1.0f / (0.030f * sr));
    widthSmoothed = 1.0f;
    sharedFastEnv = 0.0f;
    sharedSlowEnv = 0.0f;
    sampleDuration = 1.0f / sr;
    noteAgeSeconds = 10.0f;
    wasAboveOnsetThreshold = false;
}

void KeysEnhancerAudioProcessor::releaseResources()
{
}

bool KeysEnhancerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

namespace
{
    // Piecewise timed shimmer curve: silent through the initial click
    // (avoids doubling up with the note's own attack transient), blooms
    // in, holds, fades. This is what "note age" drives instead of a
    // level comparison.
    float shimmerAgeCurve (float ageSeconds) noexcept
    {
        if (ageSeconds < 0.02f)  return 0.0f;
        if (ageSeconds < 0.25f)  return (ageSeconds - 0.02f) / 0.23f;
        if (ageSeconds < 1.5f)   return 1.0f;
        if (ageSeconds < 3.0f)   return 1.0f - (ageSeconds - 1.5f) / 1.5f;
        return 0.0f;
    }
}

void KeysEnhancerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const float shimmerNorm = apvts.getRawParameterValue ("SHIMMER")->load() / 10.0f;
    const float widthNorm   = apvts.getRawParameterValue ("WIDTH")->load()   / 10.0f;
    const float mix         = apvts.getRawParameterValue ("MIX")->load();
    const float outputGain  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    if (totalNumInputChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        auto& cL = channels[0];
        auto& cR = channels[1];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dryL = left[sample];
            const float dryR = right[sample];

            const float absMono = std::abs ((dryL + dryR) * 0.5f);
            const float fastCoeff = (absMono > sharedFastEnv) ? fastAttackCoeff : fastReleaseCoeff;
            sharedFastEnv = absMono + fastCoeff * (sharedFastEnv - absMono);
            const float slowCoeff = (absMono > sharedSlowEnv) ? slowAttackCoeff : slowReleaseCoeff;
            sharedSlowEnv = absMono + slowCoeff * (sharedSlowEnv - absMono);
            const float transient = juce::jlimit (0.0f, 1.0f,
                (sharedFastEnv - sharedSlowEnv) * 4.0f / juce::jmax (sharedSlowEnv, 0.02f));

            // Onset detection just resets the age clock; the actual
            // shimmer amount comes from the timed curve above, not from
            // this transient value directly.
            const bool aboveThreshold = transient > 0.5f;
            if (aboveThreshold && ! wasAboveOnsetThreshold)
                noteAgeSeconds = 0.0f;
            wasAboveOnsetThreshold = aboveThreshold;
            noteAgeSeconds += sampleDuration;

            const float shimmerGain = shimmerNorm * shimmerAgeCurve (noteAgeSeconds) * 2.2f;
            const float shimmerL = cL.shimmerHP.processSample (0, dryL);
            const float shimmerR = cR.shimmerHP.processSample (0, dryR);
            const float preL = dryL + shimmerL * shimmerGain;
            const float preR = dryR + shimmerR * shimmerGain;

            // Width narrows toward 1.0 (no change) right on a transient,
            // and eases open toward the knob's setting between hits.
            const float widthTarget = 1.0f + widthNorm * (1.0f - transient) * 2.0f;
            widthSmoothed += (widthTarget - widthSmoothed) * widthSmoothCoeff;

            const float mid  = (preL + preR) * 0.5f;
            const float side = (preL - preR) * 0.5f * widthSmoothed;
            const float enhancedL = mid + side;
            const float enhancedR = mid - side;

            left[sample]  = (dryL * (1.0f - mix) + enhancedL * mix) * outputGain;
            right[sample] = (dryR * (1.0f - mix) + enhancedR * mix) * outputGain;
        }
    }
    else if (totalNumInputChannels == 1)
    {
        auto* data = buffer.getWritePointer (0);
        auto& c = channels[0];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dry = data[sample];
            const float absDry = std::abs (dry);
            const float fastCoeff = (absDry > sharedFastEnv) ? fastAttackCoeff : fastReleaseCoeff;
            sharedFastEnv = absDry + fastCoeff * (sharedFastEnv - absDry);
            const float slowCoeff = (absDry > sharedSlowEnv) ? slowAttackCoeff : slowReleaseCoeff;
            sharedSlowEnv = absDry + slowCoeff * (sharedSlowEnv - absDry);
            const float transient = juce::jlimit (0.0f, 1.0f,
                (sharedFastEnv - sharedSlowEnv) * 4.0f / juce::jmax (sharedSlowEnv, 0.02f));

            const bool aboveThreshold = transient > 0.5f;
            if (aboveThreshold && ! wasAboveOnsetThreshold)
                noteAgeSeconds = 0.0f;
            wasAboveOnsetThreshold = aboveThreshold;
            noteAgeSeconds += sampleDuration;

            const float shimmer = c.shimmerHP.processSample (0, dry);
            const float enhanced = dry + shimmer * shimmerNorm * shimmerAgeCurve (noteAgeSeconds) * 2.2f;
            data[sample] = (dry * (1.0f - mix) + enhanced * mix) * outputGain;
        }
    }
}

juce::AudioProcessorEditor* KeysEnhancerAudioProcessor::createEditor()
{
    return new DevKomodoUniversalEditor (*this, apvts, JucePlugin_Name, juce::Colour::fromRGB (200, 150, 255));
}

void KeysEnhancerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void KeysEnhancerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeysEnhancerAudioProcessor();
}
