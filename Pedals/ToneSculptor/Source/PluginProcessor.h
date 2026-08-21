#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"
#include <vector>

class ToneSculptorAudioProcessor  : public juce::AudioProcessor
{
public:
    ToneSculptorAudioProcessor();
    ~ToneSculptorAudioProcessor() override;

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
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    double fs = 44100.0;
    std::vector<float> lowState;
    // Same DC-blocking fix as Saturator/Preamp: Tube style (case 0 below,
    // biased = pushed + 0.08*pushed^2) has an uncompensated quadratic term
    // that leaks a low-frequency offset into the signal -- and because that
    // offset lands entirely in the lowState low-pass band, it also gets
    // amplified/attenuated by the Body knob. Blocking it before the
    // low/high split fixes both.
    std::vector<float> dcX1, dcY1;
    float dcR = 0.995f;
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    std::vector<float> driveBuffer, mixBuffer, outputGainBuffer;
    juce::dsp::Oversampling<float> oversampling { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneSculptorAudioProcessor)
};
