#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class BassEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    BassEnhancerAudioProcessor();
    ~BassEnhancerAudioProcessor() override;

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

    // HARMONICS: full-wave rectification of the low band (real technique
    // used in commercial bass enhancers like MaxxBass) so the ear infers
    // the fundamental even on speakers that can't physically reproduce it.
    // TIGHT: unlike everything else in this batch, this doesn't gate a
    // gain boost off an envelope -- it modulates a FILTER'S CUTOFF
    // frequency directly from the note's envelope. As a note decays, the
    // highpass cutoff on the low band rises, progressively shaving off
    // sub-rumble as the sustain fades -- a dynamic filter sweep, not a
    // dynamic gain effect.
    struct ChannelState
    {
        juce::dsp::LinkwitzRileyFilter<float> lowLP, highHP;
        juce::dsp::IIR::Filter<float> harmonicHP;
        juce::dsp::IIR::Filter<float> tightHP;
        float levelEnv = 0.0f;
        // Slow reference the fast levelEnv above is compared against.
        // The old version compared levelEnv to a fixed absolute number
        // (guessed without being able to listen), which could leave the
        // effect nearly silent depending on input gain staging -- this
        // makes Tight self-calibrating to the signal's own level instead.
        float slowLevelEnv = 0.0f;
        float tightCutoffSmoothed = 30.0f;
        int coeffUpdateCounter = 0;
        void reset() { lowLP.reset(); highHP.reset(); harmonicHP.reset(); tightHP.reset(); levelEnv = 0.0f; slowLevelEnv = 0.0f; tightCutoffSmoothed = 30.0f; coeffUpdateCounter = 0; }
    };
    std::vector<ChannelState> channels;
    float levelEnvCoeff = 0.0f;
    float slowLevelAttackCoeff = 0.0f, slowLevelReleaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassEnhancerAudioProcessor)
};
