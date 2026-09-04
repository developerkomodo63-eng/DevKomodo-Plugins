#pragma once

#include <JuceHeader.h>

class WaveformSynthAudioProcessor;

//==============================================================================
// Shared visual language for the plugin: dark panel background, soft rounded
// controls, one accent colour used for focus/selection/active states.
//==============================================================================
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
        auto knob = juce::Rectangle<float> (diameter, diameter)
                       .withCentre (juce::Point<float> (area.getCentreX(), area.getCentreY() - 2.0f));
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

    static inline const juce::Colour bg       = juce::Colour::fromRGB (8, 10, 14);
    static inline const juce::Colour panel    = juce::Colour::fromRGB (17, 20, 27);
    static inline const juce::Colour panel2   = juce::Colour::fromRGB (22, 26, 34);
    static inline const juce::Colour border   = juce::Colour::fromRGB (55, 61, 73);
    static inline const juce::Colour text     = juce::Colour::fromRGB (239, 242, 248);
    static inline const juce::Colour accent   = juce::Colour::fromRGB (126, 102, 255);
    static inline const juce::Colour accentB  = juce::Colour::fromRGB (91, 208, 190);
};

//==============================================================================
class WaveformSynthEditor final : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
    // A single grid page inside the tabbed area. Purely a layout container;
    // the actual sliders/combos it holds are owned elsewhere (see sliders /
    // comboBoxes below) so Page never has to worry about deleting them.
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
        juce::Array<juce::Component*> controls; // non-owning
    };

public:
    WaveformSynthEditor (WaveformSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& state)
        : AudioProcessorEditor (&p), processor (p), apvts (state), tabs (juce::TabbedButtonBar::TabsAtTop)
    {
        setOpaque (true);
        setResizable (true, true);
        setResizeLimits (820, 660, 1500, 1140);
        setLookAndFeel (&lookAndFeel);

        title.setText ("WAVEFORM", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, WaveformSynthLookAndFeel::text);
        addAndMakeVisible (title);

        subtitle.setText ("DUAL OSCILLATOR SYNTHESIZER", juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        subtitle.setColour (juce::Label::textColourId, WaveformSynthLookAndFeel::accent);
        addAndMakeVisible (subtitle);

        // IMPORTANT: every child control referenced from resized()/paint() must
        // already exist before setSize() is called below, because setSize()
        // triggers resized() immediately.
        setupPresetBar();
        buildPages();
        addAndMakeVisible (tabs);

        startTimerHz (20); // drives the live oscillator preview

        setSize (1120, 800);
    }

    ~WaveformSynthEditor() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (WaveformSynthLookAndFeel::bg);

        // Header bar (title + preset controls)
        auto top = juce::Rectangle<float> (14.0f, 14.0f, (float) getWidth() - 28.0f, 70.0f);
        g.setColour (WaveformSynthLookAndFeel::panel);
        g.fillRoundedRectangle (top, 12.0f);
        g.setColour (WaveformSynthLookAndFeel::border.withAlpha (0.7f));
        g.drawRoundedRectangle (top, 12.0f, 1.0f);

        auto accentLine = top.removeFromLeft (4.0f).reduced (0.0f, 12.0f);
        g.setColour (WaveformSynthLookAndFeel::accent);
        g.fillRoundedRectangle (accentLine, 2.0f);

        // Oscillator preview scope
        auto scope = getScopeBounds();
        g.setColour (WaveformSynthLookAndFeel::panel);
        g.fillRoundedRectangle (scope, 10.0f);
        g.setColour (WaveformSynthLookAndFeel::border.withAlpha (0.7f));
        g.drawRoundedRectangle (scope, 10.0f, 1.0f);

        drawOscilloscope (g, scope.reduced (14.0f, 10.0f));
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

        auto scope = getScopeBounds();
        const int tabsTop = (int) scope.getBottom() + 10;
        tabs.setBounds (14, tabsTop, getWidth() - 28, getHeight() - tabsTop - 14);
    }

private:
    //== construction helpers =================================================
    void setupPresetBar()
    {
        presetCombo = std::make_unique<juce::ComboBox>();
        const juce::StringArray presets { "Sub Bass", "Analog Lead", "Soft Pad", "Pluck", "Dirty Saw", "Hybrid Texture", "Glass Arp", "Bass Drone" };
        for (int i = 0; i < presets.size(); ++i)
            presetCombo->addItem (presets[i], i + 1);
        presetCombo->setSelectedId (1, juce::dontSendNotification);
        presetCombo->setTextWhenNothingSelected ("Factory preset");
        presetCombo->onChange = [this] { processor.applyPreset (presetCombo->getSelectedId() - 1); };
        presetCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (*presetCombo);

        userPresetName.setMultiLine (false);
        userPresetName.setText ("My Preset", false);
        userPresetName.setFont (juce::Font (juce::FontOptions (12.0f)));
        userPresetName.setColour (juce::TextEditor::backgroundColourId, WaveformSynthLookAndFeel::panel2);
        userPresetName.setColour (juce::TextEditor::outlineColourId, WaveformSynthLookAndFeel::border);
        userPresetName.setColour (juce::TextEditor::textColourId, WaveformSynthLookAndFeel::text);
        addAndMakeVisible (userPresetName);

        savePresetButton = std::make_unique<juce::TextButton> ("SAVE");
        deletePresetButton = std::make_unique<juce::TextButton> ("DELETE");
        for (auto* b : { savePresetButton.get(), deletePresetButton.get() })
        {
            b->setLookAndFeel (&lookAndFeel);
            addAndMakeVisible (b);
        }

        userPresetCombo = std::make_unique<juce::ComboBox>();
        userPresetCombo->setTextWhenNothingSelected ("Custom presets");
        userPresetCombo->setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (*userPresetCombo);

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

    //== oscilloscope preview ==================================================
    juce::Rectangle<float> getScopeBounds() const
    {
        return { 14.0f, 92.0f, (float) getWidth() - 28.0f, 84.0f };
    }

    // Cheap, dependency-free approximation of each wavetable bank's shape,
    // purely for the on-screen preview -- this does NOT touch the audio
    // engine or its actual wavetables, so it can never affect playback.
    static float previewShape (int waveform, float phase)
    {
        switch (juce::jlimit (0, 9, waveform))
        {
            case 0: return std::sin (juce::MathConstants<float>::twoPi * phase);
            case 1: return 2.0f * phase - 1.0f;
            case 2: return phase < 0.5f ? 1.0f : -1.0f;
            case 3: return 1.0f - 4.0f * std::abs (phase - 0.5f);
            case 4: return phase < 0.25f ? 1.0f : -1.0f;
            case 5: return 0.7f * (2.0f * phase - 1.0f)
                          + 0.3f * std::sin (juce::MathConstants<float>::twoPi * phase);
            case 6: return 0.65f * std::sin (juce::MathConstants<float>::twoPi * phase)
                          + 0.35f * std::sin (juce::MathConstants<float>::twoPi * phase * 3.0f);
            case 7: return std::sin (juce::MathConstants<float>::twoPi * phase)
                          * (1.0f - 0.35f * std::abs (std::sin (juce::MathConstants<float>::twoPi * phase * 2.0f)));
            case 8: return 1.0f - 2.0f * phase;
            default: return phase < 0.35f ? 1.0f : -1.0f;
        }
    }

    void drawOneWave (juce::Graphics& g, juce::Rectangle<float> area, int waveform,
                      juce::Colour colour, const juce::String& label) const
    {
        g.setColour (WaveformSynthLookAndFeel::border.withAlpha (0.35f));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

        juce::Path path;
        constexpr int points = 160;
        for (int i = 0; i < points; ++i)
        {
            const float x = (float) i / (float) (points - 1);
            const float value = previewShape (waveform, x);
            const auto pt = juce::Point<float> (area.getX() + x * area.getWidth(),
                                                area.getCentreY() - value * area.getHeight() * 0.42f);
            if (i == 0) path.startNewSubPath (pt);
            else        path.lineTo (pt);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (colour.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText (label, area.getX(), area.getY() - 12.0f, 60.0f, 12.0f, juce::Justification::left);
    }

    void drawOscilloscope (juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const int waveA = (int) apvts.getRawParameterValue ("OSC_A_WAVEFORM")->load();
        const int waveB = (int) apvts.getRawParameterValue ("OSC_B_WAVEFORM")->load();

        auto left  = area.removeFromLeft (area.getWidth() * 0.5f).reduced (10.0f, 0.0f);
        auto right = area.reduced (10.0f, 0.0f);

        drawOneWave (g, left,  waveA, WaveformSynthLookAndFeel::accent,  "OSC A");
        drawOneWave (g, right, waveB, WaveformSynthLookAndFeel::accentB, "OSC B");
    }

    void timerCallback() override
    {
        repaint (getScopeBounds().toNearestInt());
    }

    //== members ================================================================
    // Declaration order below is deliberate and load-bearing:
    //   1) lookAndFeel must outlive every control that points to it.
    //   2) presetCombo/userPresetCombo/savePresetButton/deletePresetButton are
    //      unique_ptr so they're never leaked and always destroyed cleanly.
    //   3) pages/sliders/comboBoxes must be destroyed BEFORE tabs is destroyed
    //      is wrong -- it's the other way around: tabs must be torn down while
    //      the Page objects it references are still valid, so tabs is declared
    //      AFTER pages/sliders/comboBoxes (members destruct in REVERSE
    //      declaration order, so the last-declared member is destroyed first).
    //   4) sliderAttachments/comboBoxAttachments must be destroyed before the
    //      sliders/combos they attach to, so they're declared last.
    WaveformSynthAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    WaveformSynthLookAndFeel lookAndFeel;

    juce::Label title, subtitle;
    std::unique_ptr<juce::ComboBox> presetCombo;
    std::unique_ptr<juce::ComboBox> userPresetCombo;
    std::unique_ptr<juce::TextButton> savePresetButton;
    std::unique_ptr<juce::TextButton> deletePresetButton;
    juce::TextEditor userPresetName;

    juce::OwnedArray<Page> pages;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> comboBoxes;
    juce::TabbedComponent tabs;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboBoxAttachments;
};