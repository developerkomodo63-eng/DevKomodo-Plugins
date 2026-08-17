#pragma once
#include <JuceHeader.h>

class SaturatorAudioProcessor final : public juce::AudioProcessor
{
public:
    SaturatorAudioProcessor();
    ~SaturatorAudioProcessor() override = default;
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
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };
private:
    juce::AudioBuffer<float> dryBuffer;
    double sampleRate = 44100.0;
    static float saturate (float x, float mode, float character) noexcept;
    // One-pole DC blocker applied after saturation. The waveshapers above
    // (especially Tube mode's quadratic term, and the Color bias that never
    // gets subtracted back out in Tube/Tape/Diode) push the signal's average
    // away from zero, which shows up as an unwanted low-frequency/rumble
    // component. Real tube/tape gear removes this with an output coupling
    // capacitor; this is the digital equivalent.
    float dcR = 0.995f;
    std::array<float, 2> dcX1 {}, dcY1 {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturatorAudioProcessor)
};
