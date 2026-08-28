#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class GuitarEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    GuitarEnhancerAudioProcessor();
    ~GuitarEnhancerAudioProcessor() override;

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

    // Not a drive/gain pedal on purpose -- if someone wants grit they'll
    // reach for Overdrive/DistortionGuitar/AmpSim. This is a mix-clarity
    // tool: CLARITY is an ADAPTIVE mud cut in the 200-500Hz box-resonance
    // region that digs in harder only when that band is actually dense
    // (strummed chords, doubled tracks) and gets out of the way for open,
    // single-note lines -- unlike a static EQ cut, which either isn't
    // enough during dense passages or thins out single notes too much.
    // PRESENCE follows a single SLOW RMS follower (how hard you're
    // playing overall), not a fast/slow transient-onset pair -- dig in
    // and play harder, the exciter opens up gradually; back off and it
    // relaxes. No attack/onset detection involved at all, deliberately a
    // different kind of "reactive" than the transient-gated approach.
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> mudBand;
        juce::dsp::LinkwitzRileyFilter<float> presenceHP;
        float mudFastEnv = 0.0f;
        float rmsEnv = 0.0f;
        // Broadband reference envelopes: both CLARITY and PRESENCE now
        // compare against the signal's OWN recent level rather than a
        // fixed absolute-amplitude number. Fixed thresholds guessed
        // without being able to listen turned out to sit above typical
        // signal levels for at least some gain-staging setups, making
        // the effect essentially inaudible -- this self-calibrates
        // regardless of how hot or quiet the incoming signal runs.
        float broadbandFastEnv = 0.0f;
        float broadbandSlowEnv = 0.0f;
        void reset()
        {
            mudBand.reset(); presenceHP.reset();
            mudFastEnv = 0.0f; rmsEnv = 0.0f;
            broadbandFastEnv = 0.0f; broadbandSlowEnv = 0.0f;
        }
    };
    std::vector<ChannelState> channels;
    float mudEnvCoeff = 0.0f;
    float rmsEnvCoeff = 0.0f;
    float broadbandSlowCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarEnhancerAudioProcessor)
};
