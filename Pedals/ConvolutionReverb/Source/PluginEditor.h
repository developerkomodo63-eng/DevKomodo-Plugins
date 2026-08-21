#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DevKomodoUI.h"

class ConvolutionReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ConvolutionReverbAudioProcessorEditor (ConvolutionReverbAudioProcessor&);
    ~ConvolutionReverbAudioProcessorEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    ConvolutionReverbAudioProcessor& audioProcessor;
    juce::TextButton loadButton { "LOAD IR" };
    juce::Label fileLabel;
    std::unique_ptr<DevKomodoUniversalEditor> editor;
    std::unique_ptr<juce::FileChooser> fileChooser;
    DevKomodoKnobLookAndFeel controlsLookAndFeel { juce::Colour::fromRGB (45, 180, 196) };
    juce::TooltipWindow tooltipWindow { this, 650 };

    void updateFileLabel();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionReverbAudioProcessorEditor)
};
