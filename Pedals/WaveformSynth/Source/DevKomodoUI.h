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
        setSize (1040, 640);

        title.setText ("WAVEFORM SYNTH", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (title);

        subtitle.setText ("DUAL OSC / LIGHT-LOAD / ANALOG INSPIRED", juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::plain)));
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
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
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
        presetCombo->onChange = [this] {
            processor.applyPreset (presetCombo->getSelectedId() - 1);
        };
        addAndMakeVisible (presetCombo);

        oscABankCombo = new juce::ComboBox ("OSC_A_BANK");
        oscABankCombo->addItem ("Classic", 1);
        oscABankCombo->addItem ("Analog", 2);
        oscABankCombo->addItem ("Digital", 3);
        oscABankCombo->addItem ("Hybrid", 4);
        addAndMakeVisible (oscABankCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_A_BANK", *oscABankCombo));

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
        addAndMakeVisible (oscAWaveformCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_A_WAVEFORM", *oscAWaveformCombo));

        oscBBankCombo = new juce::ComboBox ("OSC_B_BANK");
        oscBBankCombo->addItem ("Classic", 1);
        oscBBankCombo->addItem ("Analog", 2);
        oscBBankCombo->addItem ("Digital", 3);
        oscBBankCombo->addItem ("Hybrid", 4);
        addAndMakeVisible (oscBBankCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_B_BANK", *oscBBankCombo));

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
        addAndMakeVisible (oscBWaveformCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_B_WAVEFORM", *oscBWaveformCombo));

        filterModeCombo = new juce::ComboBox ("FILTER_MODE");
        filterModeCombo->addItem ("LP", 1);
        filterModeCombo->addItem ("BP", 2);
        filterModeCombo->addItem ("HP", 3);
        filterModeCombo->addItem ("Notch", 4);
        addAndMakeVisible (filterModeCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_MODE", *filterModeCombo));

        voiceModeCombo = new juce::ComboBox ("VOICE_MODE");
        voiceModeCombo->addItem ("Poly", 1);
        voiceModeCombo->addItem ("Mono", 2);
        voiceModeCombo->addItem ("Legato", 3);
        addAndMakeVisible (voiceModeCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "VOICE_MODE", *voiceModeCombo));

        userPresetName.setMultiLine (false);
        userPresetName.setText ("My Preset");
        userPresetName.setFont (juce::Font (juce::FontOptions (13.0f)));
        addAndMakeVisible (userPresetName);

        savePresetButton = new juce::TextButton ("SAVE_PRESET");
        savePresetButton->setButtonText ("Save");
        addAndMakeVisible (savePresetButton);

        deletePresetButton = new juce::TextButton ("DELETE_PRESET");
        deletePresetButton->setButtonText ("Delete");
        addAndMakeVisible (deletePresetButton);

        userPresetCombo = new juce::ComboBox ("USER_PRESET");
        userPresetCombo->setTextWhenNothingSelected ("Custom");
        addAndMakeVisible (userPresetCombo);

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
        addSlider ("POSITION", "POSITION", 0.0f, 1.0f, 0.42f, 2);
        addSlider ("WAVE MORPH", "WAVE_MORPH", 0.0f, 1.0f, 0.48f, 2);
        addSlider ("OSC_B_POSITION", "OSC_B_POSITION", 0.0f, 1.0f, 0.55f, 2);
        addSlider ("DETUNE", "DETUNE", 0.0f, 24.0f, 1.5f, 1);
        addSlider ("MIX", "MIX", 0.0f, 1.0f, 0.55f, 2);
        addSlider ("CUTOFF", "CUTOFF", 300.0f, 15000.0f, 3200.0f, 0);
        addSlider ("RESONANCE", "RESONANCE", 0.0f, 1.0f, 0.30f, 2);
        addSlider ("DRIVE", "DRIVE", 0.0f, 1.0f, 0.12f, 2);
        addSlider ("ATTACK", "ATTACK", 0.0f, 2.0f, 0.01f, 2);
        addSlider ("DECAY", "DECAY", 0.0f, 2.0f, 0.18f, 2);
        addSlider ("RELEASE", "RELEASE", 0.0f, 3.0f, 0.20f, 2);
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
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f, getWidth() - 16.0f, getHeight() - 16.0f), 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (juce::Rectangle<float> (12.0f, 12.0f, getWidth() - 24.0f, getHeight() - 24.0f), 12.0f, 1.0f);
        g.setColour (juce::Colour::fromRGB (35, 38, 47));
        g.fillRoundedRectangle (juce::Rectangle<float> (20.0f, 72.0f, getWidth() - 40.0f, 90.0f), 10.0f);
    }

    void resized() override
    {
        title.setBounds (24, 18, getWidth() - 48, 26);
        subtitle.setBounds (26, 48, getWidth() - 52, 18);

        const int left = 24;
        const int top = 90;
        const int comboH = 26;
        const int comboGap = 12;

        // Lay the top-row combos out sequentially instead of hardcoded
        // offsets, so widths can change without boxes overlapping.
        int x = left;
        auto placeCombo = [&] (juce::ComboBox* combo, int width)
        {
            if (combo != nullptr)
                combo->setBounds (x, top, width, comboH);
            x += width + comboGap;
        };

        placeCombo (presetCombo, 170);
        placeCombo (oscABankCombo, 120);
        placeCombo (oscAWaveformCombo, 150);
        placeCombo (oscBBankCombo, 120);
        placeCombo (oscBWaveformCombo, 150);
        placeCombo (filterModeCombo, 90);
        placeCombo (voiceModeCombo, 100);

        userPresetName.setBounds (left, top + 36, 170, 28);
        if (savePresetButton != nullptr)
            savePresetButton->setBounds (left + 182, top + 36, 70, 28);
        if (deletePresetButton != nullptr)
            deletePresetButton->setBounds (left + 260, top + 36, 72, 28);
        if (userPresetCombo != nullptr)
            userPresetCombo->setBounds (left + 342, top + 36, 170, 28);

        // 5 columns keeps 13 knobs to 3 rows, which fits inside the
        // editor height; 4 columns needed 4 rows and pushed the last
        // knob (LEVEL) below the visible window.
        const int cols = 5;
        const int gapX = 18;
        const int gapY = 18;
        const int knobWidth = (getWidth() - 90) / cols;
        const int knobHeight = 124;
        const int startX = 26;
        const int startY = 172;

        for (int i = 0; i < (int) sliders.size(); ++i)
        {
            const int knobX = startX + (i % cols) * (knobWidth + gapX);
            const int knobY = startY + (i / cols) * (knobHeight + gapY);
            sliders[(size_t) i]->setBounds (knobX, knobY, knobWidth - 18, knobHeight);
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
