#pragma once

#include <JuceHeader.h>

class WaveformSynthAudioProcessor;

// Same drawing pattern used across the rest of the DevKomodo suite
// (see Tremolo/Source/DevKomodoUI.h). WaveformSynth was missing this
// class entirely, so its combo boxes, buttons, and sliders had no
// colours/paint routine assigned to them and rendered as nothing.
class WaveformSynthLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit WaveformSynthLookAndFeel (juce::Colour accentColour = juce::Colour::fromRGB (120, 170, 255))
        : arcColour (accentColour) {}

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (14.0f, juce::Font::plain));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                        int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (18, 20, 25));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (box.isPopupActive() ? arcColour : juce::Colour::fromRGB (70, 74, 84));
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f);

        auto arrowZone = bounds.removeFromRight (20.0f);
        juce::Path arrow;
        const float cx = arrowZone.getCentreX(), cy = arrowZone.getCentreY();
        arrow.addTriangle (cx - 4.5f, cy - 2.0f, cx + 4.5f, cy - 2.0f, cx, cy + 3.5f);
        g.setColour (arcColour);
        g.fillPath (arrow);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        auto knobArea = area.reduced (8.0f, 18.0f);
        knobArea.removeFromTop (10.0f);
        const float diameter = juce::jmin (knobArea.getWidth(), knobArea.getHeight());
        auto knob = juce::Rectangle<float> (0, 0, diameter, diameter).withCentre (knobArea.getCentre());
        const float radius = knob.getWidth() * 0.5f;
        const auto centre = knob.getCentre();

        g.setColour (juce::Colour::fromRGB (18, 20, 25));
        g.fillEllipse (knob);
        g.setColour (juce::Colour::fromRGB (62, 66, 76));
        g.drawEllipse (knob, 1.0f);

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                           rotaryStartAngle, angle, true);
        g.setColour (arcColour);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float pointerLength = radius * 0.62f;
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -pointerLength, 3.0f, pointerLength * 0.42f, 1.5f);
        g.setColour (juce::Colours::white);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        auto label = slider.getName();
        if (label.isNotEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.78f));
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawFittedText (label.toUpperCase(), area.removeFromTop (18).toNearestInt(),
                              juce::Justification::centred, 1);
        }
    }

private:
    juce::Colour arcColour;
};

class WaveformSynthEditor final : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    WaveformSynthEditor (WaveformSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& state)
        : AudioProcessorEditor (&p), processor (p), apvts (state)
    {
        setOpaque (true);
        setSize (1040, 640);
        setResizable (true, true);
        setResizeLimits (860, 600, 1600, 1000);
        startTimerHz (15);

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
        presetCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (presetCombo);

        oscABankCombo = new juce::ComboBox ("OSC_A_BANK");
        oscABankCombo->addItem ("Classic", 1);
        oscABankCombo->addItem ("Analog", 2);
        oscABankCombo->addItem ("Digital", 3);
        oscABankCombo->addItem ("Hybrid", 4);
        oscABankCombo->setLookAndFeel (&lookAndFeel);
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
        oscAWaveformCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (oscAWaveformCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_A_WAVEFORM", *oscAWaveformCombo));

        oscBBankCombo = new juce::ComboBox ("OSC_B_BANK");
        oscBBankCombo->addItem ("Classic", 1);
        oscBBankCombo->addItem ("Analog", 2);
        oscBBankCombo->addItem ("Digital", 3);
        oscBBankCombo->addItem ("Hybrid", 4);
        oscBBankCombo->setLookAndFeel (&lookAndFeel);
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
        oscBWaveformCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (oscBWaveformCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "OSC_B_WAVEFORM", *oscBWaveformCombo));

        filterModeCombo = new juce::ComboBox ("FILTER_MODE");
        filterModeCombo->addItem ("LP", 1);
        filterModeCombo->addItem ("BP", 2);
        filterModeCombo->addItem ("HP", 3);
        filterModeCombo->addItem ("Notch", 4);
        filterModeCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (filterModeCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "FILTER_MODE", *filterModeCombo));

        voiceModeCombo = new juce::ComboBox ("VOICE_MODE");
        voiceModeCombo->addItem ("Poly", 1);
        voiceModeCombo->addItem ("Mono", 2);
        voiceModeCombo->addItem ("Legato", 3);
        voiceModeCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (voiceModeCombo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "VOICE_MODE", *voiceModeCombo));

        userPresetName.setMultiLine (false);
        userPresetName.setText ("My Preset");
        userPresetName.setFont (juce::Font (juce::FontOptions (13.0f)));
        userPresetName.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        userPresetName.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (14, 16, 20));
        userPresetName.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (60, 64, 70));
        userPresetName.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (120, 170, 255));
        addAndMakeVisible (userPresetName);

        savePresetButton = new juce::TextButton ("SAVE_PRESET");
        savePresetButton->setButtonText ("Save");
        savePresetButton->setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (120, 170, 255).withAlpha (0.20f));
        savePresetButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (120, 170, 255).withAlpha (0.32f));
        savePresetButton->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        savePresetButton->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (savePresetButton);

        deletePresetButton = new juce::TextButton ("DELETE_PRESET");
        deletePresetButton->setButtonText ("Delete");
        deletePresetButton->setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (120, 170, 255).withAlpha (0.20f));
        deletePresetButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (120, 170, 255).withAlpha (0.32f));
        deletePresetButton->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        deletePresetButton->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (deletePresetButton);

        userPresetCombo = new juce::ComboBox ("USER_PRESET");
        userPresetCombo->setTextWhenNothingSelected ("Custom");
        userPresetCombo->setLookAndFeel (&lookAndFeel);
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
        stopTimer();

        for (auto* slider : sliders)
            slider->setLookAndFeel (nullptr);

        for (auto* combo : { presetCombo, oscABankCombo, oscAWaveformCombo, oscBBankCombo,
                              oscBWaveformCombo, filterModeCombo, voiceModeCombo, userPresetCombo })
            if (combo != nullptr)
                combo->setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGB (10, 12, 16));
        g.setColour (juce::Colour::fromRGB (25, 28, 35));
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f, getWidth() - 16.0f, getHeight() - 16.0f), 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (juce::Rectangle<float> (12.0f, 12.0f, getWidth() - 24.0f, getHeight() - 24.0f), 12.0f, 1.0f);

        const auto display = juce::Rectangle<float> (20.0f, 72.0f, getWidth() - 40.0f, 90.0f);
        g.setColour (juce::Colour::fromRGB (35, 38, 47));
        g.fillRoundedRectangle (display, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (display.reduced (1.0f), 9.0f, 1.0f);

        drawWaveformPreview (g, display.reduced (10.0f),
                             (int) apvts.getRawParameterValue ("OSC_A_WAVEFORM")->load(),
                             juce::Colour::fromRGB (91, 208, 190), "OSC A");
        drawWaveformPreview (g, display.reduced (10.0f),
                             (int) apvts.getRawParameterValue ("OSC_B_WAVEFORM")->load(),
                             juce::Colour::fromRGB (255, 166, 92), "OSC B");
    }

    void timerCallback() override
    {
        repaint (20, 72, getWidth() - 40, 90);
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
    void drawWaveformPreview (juce::Graphics& g, juce::Rectangle<float> area, int waveform,
                              juce::Colour colour, const juce::String& label)
    {
        const bool isA = label == "OSC A";
        auto waveArea = area.withY (area.getY() + (isA ? 8.0f : 43.0f))
                            .withHeight (30.0f);

        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawHorizontalLine ((int) waveArea.getCentreY(), waveArea.getX(), waveArea.getRight());

        juce::Path path;
        constexpr int points = 220;
        for (int i = 0; i < points; ++i)
        {
            const float x = (float) i / (float) (points - 1);
            const float phase = x;
            float value = 0.0f;

            switch (juce::jlimit (0, 9, waveform))
            {
                case 0: value = std::sin (juce::MathConstants<float>::twoPi * phase); break;
                case 1: value = 2.0f * phase - 1.0f; break;
                case 2: value = phase < 0.5f ? 1.0f : -1.0f; break;
                case 3: value = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
                case 4: value = phase < 0.25f ? 1.0f : -1.0f; break;
                case 5: value = 0.7f * (2.0f * phase - 1.0f)
                                  + 0.3f * std::sin (juce::MathConstants<float>::twoPi * phase); break;
                case 6: value = 0.65f * std::sin (juce::MathConstants<float>::twoPi * phase)
                                  + 0.35f * std::sin (juce::MathConstants<float>::twoPi * phase * 3.0f); break;
                case 7: value = std::sin (juce::MathConstants<float>::twoPi * phase)
                                  * (1.0f - 0.35f * std::abs (std::sin (juce::MathConstants<float>::twoPi * phase * 2.0f))); break;
                case 8: value = 1.0f - 2.0f * phase; break;
                default: value = phase < 0.35f ? 1.0f : -1.0f; break;
            }

            const auto point = juce::Point<float> (waveArea.getX() + x * waveArea.getWidth(),
                                                   waveArea.getCentreY() - value * waveArea.getHeight() * 0.43f);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (0.72f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText (label, area.getX(), isA ? area.getY() : area.getY() + 35.0f, 42, 12, juce::Justification::left);
    }

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
    WaveformSynthLookAndFeel lookAndFeel;
};
