#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class ConsoleEQAudioProcessor  : public juce::AudioProcessor
{
public:
    ConsoleEQAudioProcessor();
    ~ConsoleEQAudioProcessor() override;

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
    // Repurposed from a plain 3-band EQ (which had no actual "console"
    // character -- just gain knobs, nothing that justified the name) into
    // a real 3-band multiband drive: split into low/mid/high via cascaded
    // Linkwitz-Riley crossovers, saturate each band independently. Drive at
    // 0 is exactly transparent for that band (dry/wet blend per band, not
    // just a small-gain tanh), so e.g. "saturate the highs, leave the rest
    // clean" is a real, exact option, not just "less saturated."
    struct ChannelSplit
    {
        juce::dsp::LinkwitzRileyFilter<float> lowLP, lowHP, midLP, midHP;
        void reset() { lowLP.reset(); lowHP.reset(); midLP.reset(); midHP.reset(); }
    };
    std::vector<ChannelSplit> splits;
    juce::AudioBuffer<float> lowBand, midBand, highBand;
    juce::SmoothedValue<float> lowDriveSmoothed, midDriveSmoothed, highDriveSmoothed, outputGainSmoothed;
    std::vector<float> lowDriveBuffer, midDriveBuffer, highDriveBuffer, outputGainBuffer;
    juce::dsp::Oversampling<float> oversampling { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleEQAudioProcessor)
};
