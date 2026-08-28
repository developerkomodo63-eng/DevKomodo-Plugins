#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class CassetteEmulationAudioProcessor : public juce::AudioProcessor
{
public:
    CassetteEmulationAudioProcessor();
    ~CassetteEmulationAudioProcessor() override;

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

    // Deliberately NOT another WOW/FLUTTER/SATURATION tape box (that's
    // already Tape Emulation). This one leans on three things that pedal
    // doesn't touch at all:
    //   - TYPE: a discrete tape-formulation switch (Ferric/Chrome/Metal),
    //     each with its own fixed bandwidth/noise floor -- a lookup
    //     table of physical characteristics, not a continuous Tone knob.
    //   - DOLBY: models noise-reduction "breathing" -- a fast encode-style
    //     envelope and a slower decode-style envelope that don't quite
    //     track each other, riding the gain by the mismatch between them.
    //     This is a genuinely different (dynamics/compander-based)
    //     mechanism from anything else in this batch.
    //   - HISS here is LEVEL-DEPENDENT: it emerges in quiet passages and
    //     gets masked by louder signal, rather than sitting at a constant
    //     level like Tape Emulation's hiss does.
    struct ChannelState
    {
        juce::dsp::IIR::Filter<float> typeFilter;
        std::vector<float> wowLine;
        int wowWritePos = 0;
        float dolbyFastEnv = 0.0f, dolbySlowEnv = 0.0f;
        float hissLevelEnv = 0.0f;
        juce::Random noiseRandom;
        void reset()
        {
            typeFilter.reset();
            std::fill (wowLine.begin(), wowLine.end(), 0.0f);
            wowWritePos = 0;
            dolbyFastEnv = 0.0f; dolbySlowEnv = 0.0f; hissLevelEnv = 0.0f;
        }
    };
    std::vector<ChannelState> channels;
    double sampleRate = 44100.0;
    float wowPhase = 0.0f;
    float dolbyFastCoeff = 0.0f, dolbySlowCoeff = 0.0f;
    float hissEnvCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CassetteEmulationAudioProcessor)
};
