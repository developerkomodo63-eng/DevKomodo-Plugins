#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"
#include <array>

class WaveformSynthAudioProcessor : public juce::AudioProcessor
{
public:
    WaveformSynthAudioProcessor();
    ~WaveformSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
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
    void applyPreset (int presetIndex);
    void saveCurrentPresetAsUserPreset (const juce::String& name);
    void loadUserPreset (const juce::String& name);
    void deleteUserPreset (const juce::String& name);
    juce::StringArray getUserPresetNames() const;
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    struct Voice
    {
        bool active = false;
        bool releasing = false;
        int midiNote = 0;
        float phase = 0.0f;
        float frequency = 0.0f;
        float targetFrequency = 0.0f;
        float amplitude = 0.0f;
        float velocity = 0.0f;
        float attackTime = 0.01f;
        float decayTime = 0.10f;
        float sustainLevel = 0.7f;
        float releaseTime = 0.20f;
        float env = 0.0f;
        float lastSample = 0.0f;
        float filterStateL = 0.0f;
        float filterStateR = 0.0f;
    };

    void buildWavetables();
    float wavetableSample (float phase, float morph, float detuneOffset = 0.0f, int bankIndex = 0) const;
    int findReusableVoice (int midiNote) const;
    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void clearHeldNotes();
    void addHeldNote (int midiNote, float velocity);
    void removeHeldNote (int midiNote);
    int getLatestHeldNote() const;

    static constexpr int tableSize = 2048;
    static constexpr int wavetableCount = 10;
    static constexpr int wavetableBanks = 4;
    std::array<std::array<std::array<float, tableSize>, wavetableCount>, wavetableBanks> wavetableBank;
    std::array<Voice, 8> voices;
    std::array<int, 16> heldNotes {};
    std::array<float, 16> heldVelocities {};
    int numHeldNotes = 0;
    juce::ValueTree customPresets { "CustomPresets" };
    double sampleRate = 44100.0;
    juce::SmoothedValue<float> levelSmoothed;
    juce::SmoothedValue<float> positionSmoothed;
    juce::SmoothedValue<float> detuneSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformSynthAudioProcessor)
};
