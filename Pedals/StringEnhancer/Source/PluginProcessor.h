#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class StringEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    StringEnhancerAudioProcessor();
    ~StringEnhancerAudioProcessor() override;

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

    // BLOOM borrows a well-established idea from physical modeling/
    // resonator plugins: a real string instrument's body has its own
    // fixed resonant frequencies (independent of the note being played --
    // that's what "sympathetic resonance" physically is). Three parallel
    // high-Q bandpass filters at fixed body-resonance-like frequencies,
    // driven by the input and mixed back in low, approximate that bloom/
    // sustain-tail character without a full modal synthesis engine. The
    // second half, SMOOTH, is the same proven ratio-triggered adaptive-
    // notch pattern used elsewhere in this batch, tuned to the ~2-4kHz
    // "shriek" register where massed/harsh bowed strings get abrasive.
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> resonator1, resonator2, resonator3;
        juce::dsp::IIR::Filter<float> shriekBand;
        float shriekEnv = 0.0f;
        void reset()
        {
            resonator1.reset(); resonator2.reset(); resonator3.reset();
            shriekBand.reset(); shriekEnv = 0.0f;
        }
    };
    std::vector<ChannelState> channels;
    float shriekEnvCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringEnhancerAudioProcessor)
};
