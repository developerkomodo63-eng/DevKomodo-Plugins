#pragma once

#include <JuceHeader.h>
#include "DemoRuntime.h"
#include <array>
#include <vector>

enum class InstrumentEnhancerType
{
    Bass,
    Guitar,
    AcousticGuitar,
    Keys
};

class InstrumentEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    explicit InstrumentEnhancerAudioProcessor (InstrumentEnhancerType type);
    ~InstrumentEnhancerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    struct ChannelBands
    {
        juce::dsp::StateVariableTPTFilter<float> body;
        juce::dsp::StateVariableTPTFilter<float> detail;
        juce::dsp::StateVariableTPTFilter<float> air;
        void reset() { body.reset(); detail.reset(); air.reset(); }
    };

    InstrumentEnhancerType type;
    double sampleRate = 44100.0;
    std::vector<ChannelBands> bands;
    juce::SmoothedValue<float> bodySmoothed, detailSmoothed, harmonicsSmoothed, attackSmoothed, sustainSmoothed;
    juce::SmoothedValue<float> mixSmoothed, levelSmoothed;
    std::vector<float> bodyBuffer, detailBuffer, harmonicsBuffer, attackBuffer, sustainBuffer, mixBuffer, levelBuffer;
    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;
    float fastAttackCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float slowAttackCoeff = 0.0f;
    float slowReleaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentEnhancerAudioProcessor)
};
