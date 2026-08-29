#pragma once

#include <JuceHeader.h>

class WaveformSynthAudioProcessor;

class WaveformSynthEditor final : public juce::AudioProcessorEditor
{
public:
    WaveformSynthEditor (WaveformSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& state)
        : AudioProcessorEditor (&p), processor (p), apvts (state)
    {
        setOpaque (true);
        setSize (980, 600);

        title.setText ("WAVEFORM SYNTH", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (title);

        subtitle.setText ("SERUM-INSPIRED / LIGHTWEIGHT / DUAL-OSC", juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::plain)));
        subtitle.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (subtitle);

        auto addSlider = [this] (const juce::String& name,
                                 const juce::String& id,
                                 float min,
                                 float max,
                                 float defaultValue,
                                 int decimals)
        {
            auto* slider = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                                             juce::Slider::TextBoxBelow);
            slider->setName (name);
            slider->setRange (min, max, decimals > 0 ? 0.01 : 0.1);
            slider->setValue (defaultValue);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
            slider->setNumDecimalPlacesToDisplay (decimals);
            slider->setVelocityBasedMode (false);
            slider->setLookAndFeel (&lookAndFeel);
            slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB (14, 16, 20));
            slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (60, 64, 70));
            slider->setTooltip (name);
            sliderAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *slider));
            sliders.emplace_back (slider);
            addAndMakeVisible (slider);
        };

        presetCombo = new juce::ComboBox ("PRESET");
        presetCombo->addItem ("Sub Bass", 1);
        presetCombo->addItem ("Analog Lead", 2);
        presetCombo->addItem ("Soft Pad", 3);
        presetCombo->addItem ("Pluck", 4);
        presetCombo->addItem ("Dirty Saw", 5);
        presetCombo->addItem ("Hybrid Texture", 6);
        presetCombo->addItem ("Glass Arp", 7);
        presetCombo->addItem ("Bass Drone", 8);
        presetCombo->setSelectedId (1);
        presetCombo->setTextWhenNothingSelected ("Preset");
        presetCombo->setTooltip ("Preset");
        presetCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "PRESET", *presetCombo));
        presetCombo->onChange = [this] {
            processor.applyPreset (presetCombo->getSelectedId() - 1);
        };
        addAndMakeVisible (presetCombo);

        oscABankCombo = new juce::ComboBox ("OSC_A_BANK");
        oscABankCombo->addItem ("Classic", 1);
        oscABankCombo->addItem ("Analog", 2);
        oscABankCombo->addItem ("Digital", 3);
        oscABankCombo->addItem ("Hybrid", 4);
        oscABankCombo->setSelectedId (1);
        oscABankCombo->setTextWhenNothingSelected ("Osc A Bank");
        oscABankCombo->setTooltip ("Osc A Bank");
        oscABankCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_A_BANK", *oscABankCombo));
        addAndMakeVisible (oscABankCombo);

        oscAWaveformCombo = new juce::ComboBox ("OSC_A_WAVEFORM");
        oscAWaveformCombo->addItem ("Sine", 1);
        oscAWaveformCombo->addItem ("Saw", 2);
        oscAWaveformCombo->addItem ("Square", 3);
        oscAWaveformCombo->addItem ("Triangle", 4);
        oscAWaveformCombo->addItem ("Pulse", 5);
        oscAWaveformCombo->addItem ("Soft Saw", 6);
        oscAWaveformCombo->addItem ("Harmonic", 7);
        oscAWaveformCombo->addItem ("Folded", 8);
        oscAWaveformCombo->addItem ("Ramp", 9);
        oscAWaveformCombo->addItem ("PWM", 10);
        oscAWaveformCombo->setSelectedId (2);
        oscAWaveformCombo->setTextWhenNothingSelected ("Osc A");
        oscAWaveformCombo->setTooltip ("Oscillator A");
        oscAWaveformCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_A_WAVEFORM", *oscAWaveformCombo));
        addAndMakeVisible (oscAWaveformCombo);

        oscBBankCombo = new juce::ComboBox ("OSC_B_BANK");
        oscBBankCombo->addItem ("Classic", 1);
        oscBBankCombo->addItem ("Analog", 2);
        oscBBankCombo->addItem ("Digital", 3);
        oscBBankCombo->addItem ("Hybrid", 4);
        oscBBankCombo->setSelectedId (2);
        oscBBankCombo->setTextWhenNothingSelected ("Osc B Bank");
        oscBBankCombo->setTooltip ("Osc B Bank");
        oscBBankCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_B_BANK", *oscBBankCombo));
        addAndMakeVisible (oscBBankCombo);

        oscBWaveformCombo = new juce::ComboBox ("OSC_B_WAVEFORM");
        oscBWaveformCombo->addItem ("Sine", 1);
        oscBWaveformCombo->addItem ("Saw", 2);
        oscBWaveformCombo->addItem ("Square", 3);
        oscBWaveformCombo->addItem ("Triangle", 4);
        oscBWaveformCombo->addItem ("Pulse", 5);
        oscBWaveformCombo->addItem ("Soft Saw", 6);
        oscBWaveformCombo->addItem ("Harmonic", 7);
        oscBWaveformCombo->addItem ("Folded", 8);
        oscBWaveformCombo->addItem ("Ramp", 9);
        oscBWaveformCombo->addItem ("PWM", 10);
        oscBWaveformCombo->setSelectedId (3);
        oscBWaveformCombo->setTextWhenNothingSelected ("Osc B");
        oscBWaveformCombo->setTooltip ("Oscillator B");
        oscBWaveformCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_B_WAVEFORM", *oscBWaveformCombo));
        addAndMakeVisible (oscBWaveformCombo);

        filterModeCombo = new juce::ComboBox ("FILTER_MODE");
        filterModeCombo->addItem ("LP", 1);
        filterModeCombo->addItem ("BP", 2);
        filterModeCombo->addItem ("HP", 3);
        filterModeCombo->addItem ("Notch", 4);
        filterModeCombo->setSelectedId (1);
        filterModeCombo->setTextWhenNothingSelected ("Filter");
        filterModeCombo->setTooltip ("Filter Mode");
        filterModeCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_MODE", *filterModeCombo));
        addAndMakeVisible (filterModeCombo);

        voiceModeCombo = new juce::ComboBox ("VOICE_MODE");
        voiceModeCombo->addItem ("Poly", 1);
        voiceModeCombo->addItem ("Mono", 2);
        voiceModeCombo->addItem ("Legato", 3);
        voiceModeCombo->setSelectedId (1);
        voiceModeCombo->setTextWhenNothingSelected ("Voice");
        voiceModeCombo->setTooltip ("Voice Mode");
        voiceModeCombo->setJustificationType (juce::Justification::centredLeft);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "VOICE_MODE", *voiceModeCombo));
        addAndMakeVisible (voiceModeCombo);

        userPresetCombo = new juce::ComboBox ("USER_PRESET");
        userPresetCombo->setTextWhenNothingSelected ("User Presets");
        userPresetCombo->setTooltip ("Saved custom presets");
        userPresetCombo->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (userPresetCombo);

        savePresetButton = new juce::TextButton ("SAVE_PRESET");
        savePresetButton->setButtonText ("Save");
        savePresetButton->setTooltip ("Save current sound as custom preset");
        savePresetButton->setName ("SAVE_PRESET_BUTTON");
        addAndMakeVisible (savePresetButton);

        deletePresetButton = new juce::TextButton ("DELETE_PRESET");
        deletePresetButton->setButtonText ("Delete");
        deletePresetButton->setTooltip ("Delete selected custom preset");
        deletePresetButton->setName ("DELETE_PRESET_BUTTON");
        addAndMakeVisible (deletePresetButton);

        userPresetName.setMultiLine (false);
        userPresetName.setText ("My Preset");
        userPresetName.setName ("USER_PRESET_NAME");
        userPresetName.setFont (juce::Font (juce::FontOptions (13.0f)));
        userPresetName.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (18, 21, 27));
        userPresetName.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        userPresetName.setColour (juce::TextEditor::highlightColourId, juce::Colour::fromRGB (85, 103, 136));
        addAndMakeVisible (userPresetName);

        savePresetButton->onClick = [this]
        {
            const auto presetName = userPresetName.getText().trim();
            if (presetName.isEmpty())
                return;

            processor.saveCurrentPresetAsUserPreset (presetName);
            refreshUserPresets();
            userPresetCombo->setText (presetName, juce::dontSendNotification);
        };

        deletePresetButton->onClick = [this]
        {
            const auto selectedText = userPresetCombo->getText();
            if (selectedText.isEmpty())
                return;

            processor.deleteUserPreset (selectedText);
            refreshUserPresets();
        };

        userPresetCombo->onChange = [this]
        {
            const auto selectedText = userPresetCombo->getText();
            if (! selectedText.isEmpty())
                processor.loadUserPreset (selectedText);
        };

        refreshUserPresets();

        addSlider ("OSC_A_POSITION", "OSC_A_POSITION", 0.0f, 1.0f, 0.35f, 2);
        addSlider ("OSC_A_PHASE", "OSC_A_PHASE", 0.0f, 1.0f, 0.12f, 2);
        addSlider ("POSITION", "POSITION", 0.0f, 1.0f, 0.42f, 2);
        addSlider ("WAVE MORPH", "WAVE_MORPH", 0.0f, 1.0f, 0.48f, 2);
        addSlider ("OSC_B_POSITION", "OSC_B_POSITION", 0.0f, 1.0f, 0.55f, 2);
        addSlider ("OSC_B_PHASE", "OSC_B_PHASE", 0.0f, 1.0f, 0.32f, 2);
        addSlider ("DETUNE", "DETUNE", 0.0f, 24.0f, 1.5f, 1);
        addSlider ("DRIFT", "DRIFT", 0.0f, 1.0f, 0.15f, 2);
        addSlider ("OSC_SYNC", "OSC_SYNC", 0.0f, 1.0f, 0.18f, 2);
        addSlider ("VIBRATO", "VIBRATO", 0.0f, 1.0f, 0.14f, 2);
        addSlider ("MOD_DEPTH", "MOD_DEPTH", 0.0f, 1.0f, 0.18f, 2);
        addSlider ("GLIDE", "GLIDE", 0.0f, 200.0f, 12.0f, 1);
        addSlider ("UNISON", "UNISON", 0.0f, 1.0f, 0.18f, 2);
        addSlider ("SUB", "SUB", 0.0f, 1.0f, 0.28f, 2);
        addSlider ("MIX", "MIX", 0.0f, 1.0f, 0.55f, 2);
        addSlider ("SPREAD", "SPREAD", 0.0f, 1.0f, 0.30f, 2);
        addSlider ("WIDTH", "WIDTH", 0.0f, 1.0f, 0.60f, 2);
        addSlider ("AIR", "AIR", 0.0f, 1.0f, 0.20f, 2);
        addSlider ("NOISE", "NOISE", 0.0f, 1.0f, 0.05f, 2);
        addSlider ("WARMTH", "WARMTH", 0.0f, 1.0f, 0.55f, 2);
        addSlider ("CHARACTER", "CHARACTER", 0.0f, 1.0f, 0.42f, 2);
        addSlider ("ATTACK", "ATTACK", 0.0f, 2.0f, 0.01f, 2);
        addSlider ("DECAY", "DECAY", 0.0f, 2.0f, 0.18f, 2);
        addSlider ("SUSTAIN", "SUSTAIN", 0.0f, 1.0f, 0.72f, 2);
        addSlider ("RELEASE", "RELEASE", 0.0f, 3.0f, 0.20f, 2);
        addSlider ("DRIVE", "DRIVE", 0.0f, 1.0f, 0.12f, 2);
        addSlider ("CUTOFF", "CUTOFF", 300.0f, 15000.0f, 3200.0f, 0);
        addSlider ("FILTER DRIVE", "FILTER_DRIVE", 0.0f, 1.0f, 0.32f, 2);
        addSlider ("RESONANCE", "RESONANCE", 0.0f, 1.0f, 0.30f, 2);
        addSlider ("LEVEL", "LEVEL", -24.0f, 12.0f, 0.0f, 1);
    }

    ~WaveformSynthEditor() override
    {
        for (auto* slider : sliders)
            slider->setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGB (10, 12, 16));
        g.setColour (juce::Colour::fromRGB (25, 28, 35));
        g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 10.0f, getWidth() - 20.0f, getHeight() - 20.0f), 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (juce::Rectangle<float> (14.0f, 14.0f, getWidth() - 28.0f, getHeight() - 28.0f), 12.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (juce::Rectangle<float> (22.0f, 72.0f, getWidth() - 44.0f, 68.0f), 10.0f);
    }

    void resized() override
    {
        title.setBounds (20, 18, getWidth() - 40, 30);
        subtitle.setBounds (30, 54, getWidth() - 60, 16);

        const int cols = 5;
        const int gap = 12;
        const int knobWidth = (getWidth() - 120) / cols;
        const int knobHeight = 120;
        const int knobStartY = 170;

        if (presetCombo != nullptr)
            presetCombo->setBounds (28, 82, 170, 30);
        if (oscABankCombo != nullptr)
            oscABankCombo->setBounds (215, 82, 125, 30);
        if (oscAWaveformCombo != nullptr)
            oscAWaveformCombo->setBounds (350, 82, 145, 30);
        if (oscBBankCombo != nullptr)
            oscBBankCombo->setBounds (505, 82, 125, 30);
        if (oscBWaveformCombo != nullptr)
            oscBWaveformCombo->setBounds (640, 82, 145, 30);
        if (filterModeCombo != nullptr)
            filterModeCombo->setBounds (800, 82, 110, 30);
        if (voiceModeCombo != nullptr)
            voiceModeCombo->setBounds (920, 82, 110, 30);

        userPresetName.setBounds (28, 122, 170, 28);
        if (savePresetButton != nullptr)
            savePresetButton->setBounds (210, 122, 64, 28);
        if (deletePresetButton != nullptr)
            deletePresetButton->setBounds (282, 122, 70, 28);
        if (userPresetCombo != nullptr)
            userPresetCombo->setBounds (360, 122, 240, 28);

        for (int i = 0; i < (int) sliders.size(); ++i)
        {
            const int x = 28 + (i % cols) * (knobWidth + gap);
            const int y = knobStartY + (i / cols) * (knobHeight + 18);
            sliders[(size_t) i]->setBounds (x, y, knobWidth - 12, knobHeight);
        }
    }

private:
    void refreshUserPresets()
    {
        if (userPresetCombo == nullptr)
            return;

        userPresetCombo->clear (juce::dontSendNotification);

        const auto presetNames = processor.getUserPresetNames();
        for (int i = 0; i < presetNames.size(); ++i)
            userPresetCombo->addItem (presetNames[i], i + 1);

        if (userPresetCombo->getNumItems() > 0)
            userPresetCombo->setSelectedId (1);
    }

    WaveformSynthAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::ComboBox* presetCombo = nullptr;
    juce::ComboBox* oscABankCombo = nullptr;
    juce::ComboBox* oscAWaveformCombo = nullptr;
    juce::ComboBox* oscBBankCombo = nullptr;
    juce::ComboBox* oscBWaveformCombo = nullptr;
    juce::ComboBox* filterModeCombo = nullptr;
    juce::ComboBox* voiceModeCombo = nullptr;
    juce::ComboBox* userPresetCombo = nullptr;
    juce::TextButton* savePresetButton = nullptr;
    juce::TextButton* deletePresetButton = nullptr;
    juce::Label title;
    juce::Label subtitle;
    juce::TextEditor userPresetName;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboBoxAttachments;
    std::vector<juce::Slider*> sliders;
    juce::LookAndFeel_V4 lookAndFeel;
};
