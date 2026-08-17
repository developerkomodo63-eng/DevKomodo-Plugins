#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DevKomodoUI.h"

class AmpSimAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit AmpSimAudioProcessorEditor (AmpSimAudioProcessor&);
    ~AmpSimAudioProcessorEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    AmpSimAudioProcessor& audioProcessor;
    juce::TextButton loadCabButton { "LOAD CAB IR" };
    juce::TextButton clearCabButton { "CLEAR IR" };
    juce::Label cabFileLabel;
    std::unique_ptr<DevKomodoUniversalEditor> editor;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool lastBassMode = false;

    void updateCabLabel();
    void updateVoiceLabels (bool bassMode);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpSimAudioProcessorEditor)
};
