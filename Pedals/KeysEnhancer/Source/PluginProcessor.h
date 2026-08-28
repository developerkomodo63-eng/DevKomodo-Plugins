#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class KeysEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    KeysEnhancerAudioProcessor();
    ~KeysEnhancerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMS", createParameterLayout() };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Two adaptive tricks, but genuinely different mechanisms: WIDTH is
    // spatial (M-S processing, unique among these four pedals). SHIMMER
    // is NOT gated by a level/transient comparison like the other three
    // pedals in this batch -- it's a scheduled ramp. A new onset resets a
    // "note age" counter to zero; shimmer amount follows a fixed timed
    // curve as that age grows (silent at the very start of the attack,
    // blooms in over the following ~250ms, holds, then fades toward the
    // release). It's driven by ELAPSED TIME since the last onset, not by
    // comparing current level against a reference.
    struct ChannelState
    {
        juce::dsp::LinkwitzRileyFilter<float> shimmerHP;
        void reset() { shimmerHP.reset(); }
    };
    std::vector<ChannelState> channels;
    float widthSmoothed = 0.0f;
    float widthSmoothCoeff = 0.0f;
    // Joint (not per-channel) envelope pair used only for the stereo-width
    // decision and for detecting new onsets to reset the age ramp.
    float sharedFastEnv = 0.0f, sharedSlowEnv = 0.0f;
    float fastAttackCoeff = 0.0f, fastReleaseCoeff = 0.0f;
    float slowAttackCoeff = 0.0f, slowReleaseCoeff = 0.0f;
    // Note-age ramp: counts seconds since the last detected onset.
    float noteAgeSeconds = 10.0f;
    float sampleDuration = 0.0f;
    bool wasAboveOnsetThreshold = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeysEnhancerAudioProcessor)
};
