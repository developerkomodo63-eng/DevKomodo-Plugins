#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class RoomEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    RoomEnhancerAudioProcessor();
    ~RoomEnhancerAudioProcessor() override;

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

    // TIGHTEN is the well-established, proven approach real "de-room"
    // tools use without a full dereverb algorithm: a downward expander
    // keyed to how far the CURRENT level has decayed below the recent
    // peak, which digs into reverb/room tails without a full impulse-
    // response-based dereverberation engine.
    //
    // DEBOOM is the original piece: rather than guessing a fixed "boxy"
    // frequency, it scans three candidate low-mid bands and tracks each
    // one with a very long (~2s) integration window, comparing it to an
    // equally slow broadband reference. A genuine room resonance gets
    // excited continuously by almost anything passing through the room,
    // so it shows up as a PERSISTENTLY elevated ratio over that long
    // window. Vocal/instrument content that happens to pass through the
    // same frequency briefly during a phrase doesn't have time to move
    // that slow an average, so it isn't mistaken for room boom. This is
    // the key idea: the TIME CONSTANT itself is the discriminator between
    // "always-on resonance" and "content that's just passing through."
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> band1, band2, band3;
        float slowEnv1 = 0.0f, slowEnv2 = 0.0f, slowEnv3 = 0.0f;
        float slowBroadbandEnv = 0.0f;
        float tailEnv = 0.0f, peakHold = 0.0f;
        void reset()
        {
            band1.reset(); band2.reset(); band3.reset();
            slowEnv1 = 0.0f; slowEnv2 = 0.0f; slowEnv3 = 0.0f;
            slowBroadbandEnv = 0.0f; tailEnv = 0.0f; peakHold = 0.0f;
        }
    };
    std::vector<ChannelState> channels;
    float slowCoeff = 0.0f;
    float tailAttackCoeff = 0.0f, tailReleaseCoeff = 0.0f;
    float peakDecayCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomEnhancerAudioProcessor)
};
