#pragma once

#include <JuceHeader.h>
#include "../../Common/DemoRuntime.h"

class DrumEnhancerAudioProcessor : public juce::AudioProcessor
{
public:
    DrumEnhancerAudioProcessor();
    ~DrumEnhancerAudioProcessor() override;

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
    // Rediseñado desde cero: la version anterior era un excitador armonico
    // multibanda ESTATICO (misma saturacion todo el tiempo, sin importar
    // si hay un golpe o no), lo cual no suena a "drum enhancer" real --
    // suena igual en el sustain de un platillo que en el ataque de un
    // kick. Un enhancer de bateria de verdad reacciona al transitorio:
    //   - PUNCH: boost dinamico en graves, proporcional al transitorio
    //     detectado (no una saturacion pareja todo el rato)
    //   - SUB: capa de sub armonico sintetizado (graves muy filtrados +
    //     saturados) para peso real de kick, no solo "mas graves"
    //   - CRACK: excitacion de agudos activada SOLO durante el
    //     transitorio, para no ensuciar el sustain de platillos/hihats
    struct ChannelState
    {
        juce::dsp::LinkwitzRileyFilter<float> lowLP, highHP;
        juce::dsp::IIR::Filter<float> subLP;
        float fastEnv = 0.0f, slowEnv = 0.0f;
        void reset() { lowLP.reset(); highHP.reset(); subLP.reset(); fastEnv = 0.0f; slowEnv = 0.0f; }
    };
    std::vector<ChannelState> channels;
    float fastAttackCoeff = 0.0f, fastReleaseCoeff = 0.0f;
    float slowAttackCoeff = 0.0f, slowReleaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEnhancerAudioProcessor)
};
