#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class VocalEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    VocalEnhancerAudioProcessor();
    ~VocalEnhancerAudioProcessor() override;

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

    // De-essing already exists as its own pedal, and pitch correction as
    // Vocal Shifter -- this isn't either of those. The detector here is
    // ZERO-CROSSING RATE, not used anywhere else in the catalog: sung/
    // spoken vowels are quasi-periodic (low ZCR), while breath and
    // consonants (s, f, t, sh, unvoiced sounds generally) are noise-like
    // (high ZCR). That single, cheap measurement is enough to tell voiced
    // from unvoiced content apart in real time, and WARMTH/AIR are routed
    // to whichever is actually happening right now instead of a fixed EQ
    // curve that's a compromise for both.
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> warmthBand;
        juce::dsp::LinkwitzRileyFilter<float> airHP;
        float lastSign = 0.0f;
        float zcrEnv = 0.0f;
        void reset() { warmthBand.reset(); airHP.reset(); lastSign = 0.0f; zcrEnv = 0.0f; }
    };
    std::vector<ChannelState> channels;
    float zcrEnvCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEnhancerAudioProcessor)
};
