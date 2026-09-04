#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"
#include "DevKomodoUI.h"

class JunoEmuVoice;

class JunoEmuAudioProcessor final : public juce::AudioProcessor
{
public:
    JunoEmuAudioProcessor();
    ~JunoEmuAudioProcessor() override;

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
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    juce::Synthesiser synth;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;
    double currentSampleRate = 44100.0;
    int lastBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JunoEmuAudioProcessor)
};

class JunoEmuVoice final : public juce::SynthesiserVoice
{
public:
    explicit JunoEmuVoice (JunoEmuAudioProcessor& owner) : processor (owner) {}

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    float oscSaw (float phase, float phaseInc) const noexcept;
    float oscPulse (float phase, float phaseInc, float width) const noexcept;
    float polyBlep (float t, float dt) const noexcept;
    float nextNoise() noexcept;
    void updateEnvelopeCoefficients();

    JunoEmuAudioProcessor& processor;
    double sampleRate = 44100.0;
    int note = 0;
    float velocity = 0.0f;
    float phase = 0.0f;
    float unisonPhaseA = 0.0f;
    float unisonPhaseB = 0.0f;
    float subPhase = 0.0f;
    float lfoPhase = 0.0f;
    float driftPhase = 0.0f;
    float driftValue = 0.0f;
    float env = 0.0f;
    float filterEnv = 0.0f;
    float currentFreq = 440.0f;
    float targetFreq = 440.0f;
    float filterL[4] {};
    float filterR[4] {};
    float hpStateL = 0.0f;
    float hpStateR = 0.0f;
    float envAttack = 0.0f;
    float envDecay = 0.0f;
    float envRelease = 0.0f;
    float filterAttack = 0.0f;
    float filterDecay = 0.0f;
    float filterRelease = 0.0f;
    bool releasing = false;
    juce::Random random;
};

class JunoEmuSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};
