#pragma once

#include <JuceHeader.h>

class WaveformSynthAudioProcessor;

class WaveformSynthLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    WaveformSynthLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.24f));
        setColour (juce::PopupMenu::highlightedTextColourId, text);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (12.0f));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f);
        g.setColour (panel2);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (box.hasKeyboardFocus (true) ? accent : border);
        g.drawRoundedRectangle (r, 7.0f, box.hasKeyboardFocus (true) ? 1.4f : 1.0f);

        auto arrow = r.removeFromRight (24.0f);
        juce::Path p;
        p.addTriangle (arrow.getCentreX() - 4.0f, arrow.getCentreY() - 2.0f,
                       arrow.getCentreX() + 4.0f, arrow.getCentreY() - 2.0f,
                       arrow.getCentreX(), arrow.getCentreY() + 3.0f);
        g.setColour (accent);
        g.fillPath (p);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        auto labelArea = area.removeFromTop (19.0f);
        g.setColour (text.withAlpha (0.72f));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawFittedText (slider.getName().toUpperCase(), labelArea.toNearestInt(), juce::Justification::centred, 1);

        const float diameter = juce::jmin (area.getWidth(), area.getHeight()) - 8.0f;
        auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (area.getCentreX(), area.getCentreY() - 2.0f);
        const auto centre = knob.getCentre();
        const float radius = diameter * 0.5f;

        g.setColour (juce::Colour::fromRGB (10, 12, 17));
        g.fillEllipse (knob);
        g.setColour (border);
        g.drawEllipse (knob, 1.0f);

        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f,
                           startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.3f, -radius * 0.66f, 2.6f, radius * 0.34f, 1.3f);
        g.setColour (juce::Colours::white);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (down ? accent.withAlpha (0.30f) : highlighted ? accent.withAlpha (0.18f) : panel2);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (highlighted ? accent : border);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        g.setColour (text);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
    }

    static inline const juce::Colour bg      = juce::Colour::fromRGB (8, 10, 14);
    static inline const juce::Colour panel   = juce::Colour::fromRGB (17, 20, 27);
    static inline const juce::Colour panel2  = juce::Colour::fromRGB (22, 26, 34);
    static inline const juce::Colour border  = juce::Colour::fromRGB (55, 61, 73);
    static inline const juce::Colour text    = juce::Colour::fromRGB (239, 242, 248);
    static inline const juce::Colour accent  = juce::Colour::fromRGB (126, 102, 255);
};

class WaveformSynthEditor final : public juce::AudioProcessorEditor
{
    class Page final : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override
        {
            g.setColour (WaveformSynthLookAndFeel::panel);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 12.0f);
            g.setColour (WaveformSynthLookAndFeel::border.withAlpha (0.65f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 12.0f, 1.0f);
        }

        void resized() override
        {
            const int margin = 20;
            const int gap = 10;
            const int cols = getWidth() >= 900 ? 5 : getWidth() >= 680 ? 4 : 3;
            const int cellW = juce::jmax (100, (getWidth() - margin * 2 - gap * (cols - 1)) / cols);
            const int cellH = 112;

            for (int i = 0; i < controls.size(); ++i)
            {
                const int row = i / cols;
                const int col = i % cols;
                controls[i]->setBounds (margin + col * (cellW + gap),
                                        margin + row * (cellH + gap), cellW, cellH);
            }
        }

        void addControl (juce::Component* c)
        {
            controls.add (c);
            addAndMakeVisible (c);
        }

    private:
        juce::Array<juce::Component*> controls;
    };

public:
    WaveformSynthEditor (WaveformSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& state)
        : AudioProcessorEditor (&p), processor (p), apvts (state), tabs (juce::TabbedButtonBar::TabsAtTop)
    {
        setOpaque (true);
        setResizable (true, true);
        setResizeLimits (820, 620, 1500, 1100);
        setSize (1120, 760);
        setLookAndFeel (&lookAndFeel);

        title.setText ("WAVEFORM", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, WaveformSynthLookAndFeel::text);
        addAndMakeVisible (title);

        subtitle.setText ("DUAL OSCILLATOR SYNTHESIZER", juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        subtitle.setColour (juce::Label::textColourId, WaveformSynthLookAndFeel::accent);
        addAndMakeVisible (subtitle);

        setupPresetBar();
        buildPages();
        addAndMakeVisible (tabs);
    }

    ~WaveformSynthEditor() override
    {
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (WaveformSynthLookAndFeel::bg);

        auto top = juce::Rectangle<float> (14.0f, 14.0f, (float) getWidth() - 28.0f, 70.0f);
        g.setColour (WaveformSynthLookAndFeel::panel);
        g.fillRoundedRectangle (top, 12.0f);
        g.setColour (WaveformSynthLookAndFeel::border.withAlpha (0.7f));
        g.drawRoundedRectangle (top, 12.0f, 1.0f);

        auto accentLine = top.removeFromLeft (4.0f).reduced (0.0f, 12.0f);
        g.setColour (WaveformSynthLookAndFeel::accent);
        g.fillRoundedRectangle (accentLine, 2.0f);
    }

    void resized() override
    {
        title.setBounds (30, 23, 150, 28);
        subtitle.setBounds (31, 50, 220, 17);

        const int right = getWidth() - 30;
        const int presetW = 190;
        const int nameW = 180;
        const int buttonW = 70;
        const int gap = 8;
        const int barY = 27;
        const int barH = 30;

        presetCombo->setBounds (right - presetW, barY, presetW, barH);
        userPresetName.setBounds (right - presetW - gap - nameW, barY, nameW, barH);
        userPresetCombo->setBounds (right - presetW - gap * 4 - nameW - buttonW * 2 - 170, barY, 170, barH);
        savePresetButton->setBounds (right - presetW - gap * 2 - nameW - buttonW, barY, buttonW, barH);
        deletePresetButton->setBounds (right - presetW - gap * 3 - nameW - buttonW * 2, barY, buttonW, barH);

        tabs.setBounds (14, 94, getWidth() - 28, getHeight() - 108);
    }

private:
    void setupPresetBar()
    {
        presetCombo = new juce::ComboBox();
        const juce::StringArray presets { "Sub Bass", "Analog Lead", "Soft Pad", "Pluck", "Dirty Saw", "Hybrid Texture", "Glass Arp", "Bass Drone" };
        for (int i = 0; i < presets.size(); ++i)
            presetCombo->addItem (presets[i], i + 1);
        presetCombo->setSelectedId (1, juce::dontSendNotification);
        presetCombo->setTextWhenNothingSelected ("Factory preset");
        presetCombo->onChange = [this] { processor.applyPreset (presetCombo->getSelectedId() - 1); };
        presetCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (presetCombo);

        userPresetName.setMultiLine (false);
        userPresetName.setText ("My Preset", false);
        userPresetName.setFont (juce::Font (juce::FontOptions (12.0f)));
        userPresetName.setColour (juce::TextEditor::backgroundColourId, WaveformSynthLookAndFeel::panel2);
        userPresetName.setColour (juce::TextEditor::outlineColourId, WaveformSynthLookAndFeel::border);
        userPresetName.setColour (juce::TextEditor::textColourId, WaveformSynthLookAndFeel::text);
        addAndMakeVisible (userPresetName);

        savePresetButton = new juce::TextButton ("SAVE");
        deletePresetButton = new juce::TextButton ("DELETE");
        for (auto* b : { savePresetButton, deletePresetButton })
        {
            b->setLookAndFeel (&lookAndFeel);
            addAndMakeVisible (b);
        }

        userPresetCombo = new juce::ComboBox();
        userPresetCombo->setTextWhenNothingSelected ("Custom presets");
        userPresetCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (userPresetCombo);

        savePresetButton->onClick = [this]
        {
            const auto name = userPresetName.getText().trim();
            if (name.isNotEmpty())
            {
                processor.saveCurrentPresetAsUserPreset (name);
                refreshUserPresets();
                userPresetCombo->setText (name, juce::dontSendNotification);
            }
        };

        deletePresetButton->onClick = [this]
        {
            const auto name = userPresetCombo->getText().trim();
            if (name.isNotEmpty())
            {
                processor.deleteUserPreset (name);
                refreshUserPresets();
            }
        };

        userPresetCombo->onChange = [this]
        {
            const auto name = userPresetCombo->getText().trim();
            if (name.isNotEmpty())
                processor.loadUserPreset (name);
        };

        refreshUserPresets();
    }

    juce::ComboBox* addChoice (Page& page, const juce::String& id, const juce::StringArray& items)
    {
        auto* combo = new juce::ComboBox();
        for (int i = 0; i < items.size(); ++i)
            combo->addItem (items[i], i + 1);
        combo->setTextWhenNothingSelected (id);
        combo->setLookAndFeel (&lookAndFeel);
        page.addControl (combo);
        comboBoxAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *combo));
        comboBoxes.add (combo);
        return combo;
    }

    juce::Slider* addSlider (Page& page, const juce::String& id)
    {
        auto* slider = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        slider->setName (apvts.getParameter (id)->getName (100));
        const auto range = apvts.getParameter (id)->getNormalisableRange();
        slider->setRange (range.start, range.end, range.interval);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 20);
        slider->setNumDecimalPlacesToDisplay (range.interval >= 1.0f ? 0 : range.interval >= 0.1f ? 1 : 2);
        slider->setVelocityBasedMode (false);
        slider->setDoubleClickReturnValue (true, range.convertFrom0to1 (apvts.getParameter (id)->getDefaultValue()));
        slider->setLookAndFeel (&lookAndFeel);
        slider->setColour (juce::Slider::textBoxTextColourId, WaveformSynthLookAndFeel::text);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, WaveformSynthLookAndFeel::panel2);
        slider->setColour (juce::Slider::textBoxOutlineColourId, WaveformSynthLookAndFeel::border);
        slider->setTooltip (id);

        if (id == "CUTOFF") slider->setTextValueSuffix (" Hz");
        else if (id == "LEVEL") slider->setTextValueSuffix (" dB");
        else if (id == "DETUNE") slider->setTextValueSuffix (" st");
        else if (id == "GLIDE") slider->setTextValueSuffix (" ms");
        else if (id == "ATTACK" || id == "DECAY" || id == "RELEASE") slider->setTextValueSuffix (" s");

        page.addControl (slider);
        sliderAttachments.emplace_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *slider));
        sliders.add (slider);
        return slider;
    }

    void buildPages()
    {
        auto oscPage = std::make_unique<Page>();
        auto modPage = std::make_unique<Page>();
        auto filterPage = std::make_unique<Page>();

        const juce::StringArray banks { "Classic", "Analog", "Digital", "Hybrid" };
        const juce::StringArray waves { "Sine", "Saw", "Square", "Triangle", "Pulse", "Soft Saw", "Harmonic", "Folded", "Ramp", "PWM" };
        const juce::StringArray filterModes { "LP", "BP", "HP", "Notch" };
        const juce::StringArray voiceModes { "Poly", "Mono", "Legato" };

        addChoice (*oscPage, "OSC_A_BANK", banks);
        addChoice (*oscPage, "OSC_A_WAVEFORM", waves);
        addSlider (*oscPage, "OSC_A_POSITION");
        addSlider (*oscPage, "OSC_A_PHASE");
        addChoice (*oscPage, "OSC_B_BANK", banks);
        addChoice (*oscPage, "OSC_B_WAVEFORM", waves);
        addSlider (*oscPage, "OSC_B_POSITION");
        addSlider (*oscPage, "OSC_B_PHASE");
        addSlider (*oscPage, "POSITION");
        addSlider (*oscPage, "WAVE_MORPH");
        addSlider (*oscPage, "DETUNE");

        for (const auto& id : juce::StringArray { "DRIFT", "OSC_SYNC", "VIBRATO", "MOD_DEPTH", "UNISON", "SUB", "MIX", "SPREAD", "WIDTH", "AIR", "NOISE", "WARMTH", "CHARACTER" })
            addSlider (*modPage, id);

        addChoice (*filterPage, "VOICE_MODE", voiceModes);
        addSlider (*filterPage, "GLIDE");
        addChoice (*filterPage, "FILTER_MODE", filterModes);
        addSlider (*filterPage, "CUTOFF");
        addSlider (*filterPage, "RESONANCE");
        addSlider (*filterPage, "FILTER_DRIVE");
        addSlider (*filterPage, "DRIVE");
        addSlider (*filterPage, "ATTACK");
        addSlider (*filterPage, "DECAY");
        addSlider (*filterPage, "SUSTAIN");
        addSlider (*filterPage, "RELEASE");
        addSlider (*filterPage, "LEVEL");

        pages.add (oscPage.release());
        pages.add (modPage.release());
        pages.add (filterPage.release());

        tabs.addTab ("OSCILLATORS", WaveformSynthLookAndFeel::accent, pages[0], false);
        tabs.addTab ("MODULATION", WaveformSynthLookAndFeel::accent, pages[1], false);
        tabs.addTab ("FILTER / ENV", WaveformSynthLookAndFeel::accent, pages[2], false);
        tabs.setCurrentTabIndex (0, false);
    }

    void refreshUserPresets()
    {
        userPresetCombo->clear (juce::dontSendNotification);
        const auto names = processor.getUserPresetNames();
        for (int i = 0; i < names.size(); ++i)
            userPresetCombo->addItem (names[i], i + 1);
    }

    WaveformSynthAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    WaveformSynthLookAndFeel lookAndFeel;

    juce::Label title, subtitle;
    juce::ComboBox* presetCombo = nullptr;
    juce::ComboBox* userPresetCombo = nullptr;
    juce::TextButton* savePresetButton = nullptr;
    juce::TextButton* deletePresetButton = nullptr;
    juce::TextEditor userPresetName;
    juce::TabbedComponent tabs;
    juce::OwnedArray<Page> pages;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> comboBoxes;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboBoxAttachments;
};
