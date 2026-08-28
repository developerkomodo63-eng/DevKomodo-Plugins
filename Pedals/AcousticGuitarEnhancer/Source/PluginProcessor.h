#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class AcousticGuitarEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    AcousticGuitarEnhancerAudioProcessor();
    ~AcousticGuitarEnhancerAudioProcessor() override;

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

    // Genuinely different math from Guitar Enhancer's single-band-vs-
    // broadband ratio: DEBOX compares TWO adjacent bands (the box-
    // resonance region against a slightly higher reference band) and
    // cuts based on the TILT/imbalance between them -- a two-band slope
    // comparison, not a band-vs-total-energy ratio. SPARKLE is gated by
    // CREST FACTOR (peak-to-RMS ratio in a short window) rather than the
    // fast/slow exponential envelope pair used elsewhere -- a pick/strum
    // attack has a high crest factor (a sharp peak against low recent
    // average energy), a sustained tone has a low one, so this tracks
    // "spikiness" directly instead of inferring it from envelope timing.
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> boxBand, referenceBand;
        juce::dsp::LinkwitzRileyFilter<float> sparkleHP;
        float boxEnv = 0.0f, referenceEnv = 0.0f;
        float peakHold = 0.0f, rmsSquaredEnv = 0.0f;
        void reset()
        {
            boxBand.reset(); referenceBand.reset(); sparkleHP.reset();
            boxEnv = 0.0f; referenceEnv = 0.0f; peakHold = 0.0f; rmsSquaredEnv = 0.0f;
        }
    };
    std::vector<ChannelState> channels;
    float envCoeff = 0.0f;
    float peakDecayCoeff = 0.0f;
    float rmsCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticGuitarEnhancerAudioProcessor)
};
