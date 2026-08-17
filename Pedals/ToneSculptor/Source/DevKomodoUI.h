#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>


class DevKomodoKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit DevKomodoKnobLookAndFeel (juce::Colour accentColour = juce::Colour::fromRGB (111, 218, 175))
        : arcColour (accentColour) {}

    void setArcColour (juce::Colour c) { arcColour = c; }

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
        if (label.isEmpty())
            label = slider.getComponentID();
        if (label.isNotEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.78f));
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            g.drawFittedText (label.toUpperCase(), area.removeFromTop (20).toNearestInt(),
                              juce::Justification::centred, 1);
        }
    }

private:
    juce::Colour arcColour;
};

// Shared, header-only commercial-style editor used by the pedals that do not
// need a bespoke visualizer. It intentionally lives inside each plugin's
// Source directory when installed so the existing CMake/build structure stays
// unchanged.
class DevKomodoUniversalEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    DevKomodoUniversalEditor (juce::AudioProcessor& processor,
                              juce::AudioProcessorValueTreeState& state,
                              juce::String productName,
                              juce::Colour accentColour = juce::Colour::fromRGB (111, 218, 175))
        : AudioProcessorEditor (&processor), processorRef (processor), apvts (state), name (std::move (productName)), accent (accentColour), knobLookAndFeel (accentColour)
    {
        setOpaque (true);
        setResizable (true, true);
        setResizeLimits (560, 330, 1180, 760);

        title.setText (name.isNotEmpty() ? name.toUpperCase() : "DEVKOMODO", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        title.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (title);

        const bool premiumProduct = name.toUpperCase().contains ("CLEANUP PRO")
                                  || name.toUpperCase().contains ("TONE SCULPTOR")
                                  || name.toUpperCase().contains ("AMPSIM")
                                  || name.toUpperCase().contains ("CONVOLUTION REVERB");
        brand.setText (premiumProduct ? "DEVKOMODO  •  PREMIUM" : "DEVKOMODO", juce::dontSendNotification);
        brand.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        // Blend the accent with white (rather than using the raw accent at
        // partial alpha) so the brand text stays clearly legible even for
        // darker/muted accent colours -- the old low-alpha accent text was
        // hard to read against the dark header for several pedals.
        brand.setColour (juce::Label::textColourId, juce::Colours::white.interpolatedWith (accent, 0.55f));
        brand.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (brand);

        presetBox.addItem ("MANUAL", 1);
        factoryPresets = makePresets();
        for (int i = 0; i < (int) factoryPresets.size(); ++i)
            presetBox.addItem (factoryPresets[(size_t) i].name, i + 2);
        presetBox.setSelectedId (1, juce::dontSendNotification);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id >= 2 && id - 2 < (int) factoryPresets.size())
                applyPreset (factoryPresets[(size_t) id - 2]);
        };
        presetBox.setTooltip ("Select a factory preset or MANUAL");
        addAndMakeVisible (presetBox);

        if (apvts.getParameter ("INSTRUMENT") != nullptr)
        {
            instrumentButton.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.18f));
            instrumentButton.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.30f));
            instrumentButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            instrumentButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            instrumentButton.setButtonText ("GUITAR");
            instrumentButton.setClickingTogglesState (false);
            instrumentButton.onClick = [this]
            {
                if (auto* p = apvts.getParameter ("INSTRUMENT"))
                {
                    const float current = p->getValue();
                    p->setValueNotifyingHost (current > 0.5f ? 0.0f : 1.0f);
                }
                refreshInstrumentButton();
            };
            instrumentButton.setTooltip ("Switch between Guitar and Bass processing");
            addAndMakeVisible (instrumentButton);
            refreshInstrumentButton();
            startTimerHz (12);
        }

        for (auto* parameter : processorRef.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            if (ranged == nullptr)
                continue;

            const auto id = ranged->paramID;
            if (id == "INSTRUMENT")
            {
                continue;
            }
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (ranged))
            {
                Choice c;
                c.label = std::make_unique<juce::Label>();
                c.label->setText (ranged->name, juce::dontSendNotification);
                c.label->setJustificationType (juce::Justification::centred);
                c.label->setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                c.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));

                c.box = std::make_unique<juce::ComboBox>();
                c.box->addItemList (choice->choices, 1);
                c.box->setTooltip (ranged->name.isNotEmpty() ? ranged->name : prettifyID (id));
                c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                    (apvts, id, *c.box);
                c.paramID = id;

                addAndMakeVisible (*c.label);
                addAndMakeVisible (*c.box);
                choices.push_back (std::move (c));
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
            {
                Toggle t;
                t.button = std::make_unique<juce::ToggleButton> (ranged->name);
                t.button->setTooltip (ranged->name.isNotEmpty() ? ranged->name : prettifyID (id));
                t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                    (apvts, id, *t.button);
                addAndMakeVisible (*t.button);
                toggles.push_back (std::move (t));
            }
            else
            {
                Knob k;
                k.label = std::make_unique<juce::Label>();
                k.label->setText (ranged->name, juce::dontSendNotification);
                k.label->setJustificationType (juce::Justification::centred);
                k.label->setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                k.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));

                k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                            juce::Slider::TextBoxBelow);
                k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 22);
                k.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.90f));
                k.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB (14, 16, 20));
                k.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (55, 60, 70));
                k.slider->setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.25f));
                k.slider->setTooltip (ranged->name.isNotEmpty() ? ranged->name : prettifyID (id));
                const auto upperID = id.toUpperCase();
                const bool showPercent = (upperID == "MIX" || upperID == "DEPTH" || upperID == "WIDTH"
                                       || upperID == "WET" || upperID == "FEEDBACK" || upperID == "AMOUNT"
                                       || upperID == "SCOOP" || upperID == "TRANS" || upperID == "PULSEWIDTH");
                if (showPercent && ranged->getNormalisableRange().start == 0.0f
                    && ranged->getNormalisableRange().end <= 1.0f)
                {
                    k.slider->setNumDecimalPlacesToDisplay (0);
                    k.slider->textFromValueFunction = [] (double value)
                    { return juce::String (juce::roundToInt ((float) value * 100.0f)) + "%"; };
                    k.slider->valueFromTextFunction = [] (const juce::String& text)
                    { return text.trimCharactersAtEnd ("% ").getDoubleValue() * 0.01; };
                }
                else
                {
                    k.slider->setTextValueSuffix (ranged->label);
                }
                k.slider->setScrollWheelEnabled (false);
                // 320 px gives noticeably finer control than the old 220 px
                // while still feeling quick on 0-10 amp-style controls.
                k.slider->setMouseDragSensitivity (320);
                k.slider->setVelocityBasedMode (false);
                
                // RangedAudioParameter does not expose its display precision in JUCE 8.
                // Infer a lightweight, stable precision from the parameter interval.
                {
                    const auto range = ranged->getNormalisableRange();
                    const auto interval = std::abs (range.interval);
                    int decimals = 0;
                    if (interval > 0.0f && interval < 1.0f)
                        decimals = interval >= 0.1f ? 1 : (interval >= 0.01f ? 2 : 3);
                    k.slider->setNumDecimalPlacesToDisplay (decimals);
                }
                if (! showPercent)
                {
                    const auto upper = id.toUpperCase();
                    const auto range = ranged->getNormalisableRange();
                    if (upper.contains ("FREQ") || upper.contains ("TUNING"))
                        k.slider->setTextValueSuffix (" Hz");
                    else if (upper.contains ("TIME") || upper.contains ("DELAY") || upper.contains ("ATTACK")
                          || upper.contains ("RELEASE") || upper.contains ("DECAY") || upper.contains ("GLIDE"))
                        k.slider->setTextValueSuffix (" ms");
                    else if (range.start >= 0.0f && range.end <= 10.0f
                          && (upper.contains ("DRIVE") || upper == "GAIN" || upper == "LEVEL"
                              || upper == "BASS" || upper == "MID" || upper == "TREBLE"
                              || upper == "PRESENCE" || upper == "TONE" || upper == "CHARACTER"))
                        k.slider->setTextValueSuffix (" / 10");
                    else if (upper.contains ("LEVEL") || upper.contains ("OUTPUT") || upper.contains ("MAKEUP")
                          || upper.contains ("THRESH") || upper.contains ("GATE") || upper == "GAIN"
                          || upper.contains ("BODY") || upper.contains ("AIR"))
                        k.slider->setTextValueSuffix (" dB");
                }
                k.slider->setName (prettifyID (id));
                k.slider->setDoubleClickReturnValue (true,
                    ranged->getNormalisableRange().convertFrom0to1 (ranged->getDefaultValue()));
                k.slider->setLookAndFeel (&knobLookAndFeel);
                k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                    (apvts, id, *k.slider);

                addAndMakeVisible (*k.label);
                addAndMakeVisible (*k.slider);
                knobs.push_back (std::move (k));
            }
        }

        // setSize() must come after every child control exists: it triggers
        // resized() synchronously, and resized() lays out knobs/labels/boxes
        // from the vectors above. Calling it earlier (as this used to) fired
        // resized() while those vectors were still empty, so nothing had
        // real bounds until the host forced a second resize -- which is why
        // the UI looked blank/empty until you dragged to resize the window.
        setSize (760, 470);
    }

    ~DevKomodoUniversalEditor() override
    {
        stopTimer();
        for (auto& k : knobs)
            if (k.slider != nullptr) k.slider->setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGB (9, 10, 13));

        auto outer = getLocalBounds().toFloat().reduced (14.0f);
        g.setColour (juce::Colour::fromRGB (27, 29, 35));
        g.fillRoundedRectangle (outer, 13.0f);
        g.setColour (juce::Colour::fromRGB (55, 58, 68));
        g.drawRoundedRectangle (outer, 13.0f, 1.0f);

        // Static card shading is paint-only; it adds no DSP cost. The card
        // rectangles are cached from resized() so they always line up exactly
        // with the real knob/combo bounds (previously this recomputed its own
        // grid with different margins, so the cards visibly drifted away from
        // the actual controls).
        for (const auto& cell : controlCellBounds)
        {
            auto card = cell.toFloat();
            g.setColour (juce::Colour::fromRGB (22, 24, 29));
            g.fillRoundedRectangle (card, 9.0f);
            g.setColour (juce::Colour::fromRGB (48, 52, 61));
            g.drawRoundedRectangle (card, 9.0f, 1.0f);
        }

        auto header = outer.removeFromTop (64.0f);
        g.setColour (juce::Colour::fromRGB (21, 23, 28));
        g.fillRoundedRectangle (header, 12.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (header.getX(), header.getY(), 4.0f, header.getHeight(), 2.0f);

        auto meter = outer.removeFromTop (34.0f).reduced (10.0f, 7.0f);
        g.setColour (juce::Colour::fromRGB (16, 17, 21));
        g.fillRoundedRectangle (meter, 7.0f);
        g.setColour (juce::Colour::fromRGB (65, 68, 77));
        g.drawRoundedRectangle (meter, 7.0f, 1.0f);

        // This is intentionally a status strip, not a fake audio meter. The
        // generic editor has no safe real-time access to the processor buffer,
        // so showing a hard-coded level would be misleading.
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (meter.withWidth (juce::jmin (meter.getWidth() * 0.18f, 42.0f)), 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("READY", meter.reduced (8.0f, 0.0f), juce::Justification::left);

        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (juce::String (knobs.size() + choices.size() + toggles.size()) + " CONTROLS",
                    meter.reduced (8.0f, 0.0f), juce::Justification::right);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (18, 14);
        auto header = bounds.removeFromTop (62);

        // Responsive header: the old left/right carving could overlap at the
        // minimum editor width (title + brand + preset + instrument exceeded
        // the available width). Keep the two action controls on the right and
        // stack the title/brand on the left so every size remains usable.
        auto rightHeader = header.removeFromRight (instrumentButton.isVisible() ? 286 : 176);
        if (instrumentButton.isVisible())
            instrumentButton.setBounds (rightHeader.removeFromRight (108).reduced (3, 10));
        presetBox.setBounds (rightHeader.removeFromRight (170).reduced (3, 10));

        auto titleArea = header.reduced (2, 2);
        title.setBounds (titleArea.removeFromTop (34));
        brand.setBounds (titleArea);

        bounds.removeFromTop (12);
        auto footer = bounds.removeFromBottom (30);
        bounds.removeFromBottom (8);

        std::vector<juce::Component*> controls;
        std::vector<juce::Component*> labels;
        controls.reserve (knobs.size() + choices.size());
        labels.reserve (knobs.size() + choices.size());

        for (auto& k : knobs)
        {
            controls.push_back (k.slider.get());
            labels.push_back (k.label.get());
        }
        for (auto& c : choices)
        {
            controls.push_back (c.box.get());
            labels.push_back (c.label.get());
        }

        controlCellBounds.clear();
        const int count = (int) controls.size();
        if (count > 0)
        {
            const int columns = count <= 3 ? count : (count <= 6 ? 3 : 4);
            const int rows = (count + columns - 1) / columns;
            const int gap = 8;
            const int cellW = juce::jmax (92, (bounds.getWidth() - gap * (columns - 1)) / columns);
            const int cellH = juce::jmax (82, (bounds.getHeight() - gap * (rows - 1)) / rows);

            controlCellBounds.reserve ((size_t) count);
            for (int i = 0; i < count; ++i)
            {
                const int row = i / columns;
                const int col = i % columns;
                auto cell = juce::Rectangle<int> (bounds.getX() + col * (cellW + gap),
                                                   bounds.getY() + row * (cellH + gap),
                                                   cellW, cellH);
                controlCellBounds.push_back (cell);
                auto labelArea = cell.removeFromTop (20);
                labels[(size_t) i]->setBounds (labelArea.reduced (3, 0));
                controls[(size_t) i]->setBounds (cell.reduced (6, 2));
            }
        }

        if (! toggles.empty())
        {
            const int w = juce::jmax (90, footer.getWidth() / (int) toggles.size());
            for (auto& t : toggles)
                t.button->setBounds (footer.removeFromLeft (w).reduced (3, 2));
        }
    }

    // Lets an owning editor (e.g. AmpSim swapping "Amp Voice" between guitar
    // and bass amp names) replace a combo box's visible item list in place,
    // without touching the underlying parameter's index/range.
    void relabelChoiceItems (const juce::String& paramIDToMatch, const juce::StringArray& newLabels)
    {
        for (auto& c : choices)
        {
            if (c.box == nullptr || c.paramID != paramIDToMatch)
                continue;
            const int currentId = c.box->getSelectedId();
            c.box->clear (juce::dontSendNotification);
            c.box->addItemList (newLabels, 1);
            if (currentId > 0)
                c.box->setSelectedId (currentId, juce::dontSendNotification);
        }
    }


private:
    void timerCallback() override
    {
        refreshInstrumentButton();
    }

    void refreshInstrumentButton()
    {
        if (auto* p = apvts.getParameter ("INSTRUMENT"))
            instrumentButton.setButtonText (p->getValue() > 0.5f ? "BASS" : "GUITAR");
    }

    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
        juce::String paramID;
    };

    struct Toggle
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    static juce::String upper (juce::String value)
    {
        return value.toUpperCase();
    }

    static juce::String prettifyID (const juce::String& id)
    {
        juce::String s = id;
        s = s.replaceCharacter ('_', ' ').replaceCharacter ('-', ' ');
        juce::StringArray parts;
        parts.addTokens (s, " ", "");
        for (int i = 0; i < parts.size(); ++i)
        {
            auto p = parts[i].trim();
            if (p.isEmpty())
                continue;
            p = p.toLowerCase();
            if (p.length() > 0)
                p = p.substring (0, 1).toUpperCase() + p.substring (1);
            parts.set (i, p);
        }
        return parts.joinIntoString (" ");
    }


    juce::AudioProcessorParameter* findParameterById (const juce::String& id) const
    {
        for (auto* parameter : processorRef.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (ranged->paramID == id)
                    return ranged;

        return nullptr;
    }

    float defaultNormalised (juce::RangedAudioParameter& parameter) const
    {
        return juce::jlimit (0.0f, 1.0f, parameter.getDefaultValue());
    }

    float adjustForPreset (const juce::String& id, float base, int preset) const
    {
        const auto key = upper (id);
        float value = base;

        const bool drive = key.contains ("DRIVE") || key.contains ("FUZZ") || key.contains ("DISTORT")
                        || key.contains ("SATURATION") || key.contains ("BIAS") || key == "GAIN";
        const bool wet = key.contains ("MIX") || key.contains ("DEPTH") || key.contains ("FEEDBACK")
                      || key.contains ("WIDTH") || key.contains ("RESONANCE");
        const bool time = key.contains ("TIME") || key.contains ("DELAY") || key.contains ("RELEASE")
                       || key.contains ("SWELL") || key.contains ("DECAY");
        const bool tone = key.contains ("TONE") || key.contains ("TREBLE") || key.contains ("HIGH")
                       || key.contains ("PRESENCE") || key.contains ("FREQUENCY");
        const bool level = key.contains ("LEVEL") || key.contains ("OUTPUT") || key.contains ("MAKEUP");
        const bool gate = key.contains ("GATE") || key.contains ("THRESHOLD");

        switch (preset)
        {
            case 0: // Factory Reset: defaults.
                break;
            case 1: // Clean / controlled.
                if (drive) value *= 0.45f;
                if (wet) value = juce::jmap (value, 0.0f, 1.0f, 0.15f, 0.45f);
                if (tone) value = juce::jmap (value, 0.0f, 1.0f, 0.42f, 0.62f);
                if (level) value = juce::jmap (value, 0.0f, 1.0f, 0.45f, 0.58f);
                break;
            case 2: // Punch.
                if (drive) value = juce::jmax (value, 0.65f);
                if (wet) value = juce::jlimit (0.0f, 1.0f, value * 1.15f);
                if (time) value *= 0.75f;
                if (gate) value = juce::jlimit (0.0f, 1.0f, value * 0.85f);
                break;
            case 3: // Wide / ambient.
                if (wet) value = juce::jmax (value, 0.72f);
                if (time) value = juce::jmax (value, 0.55f);
                if (tone) value = juce::jmin (1.0f, value * 1.10f);
                break;
            default: // Extreme / modern.
                if (drive) value = juce::jmax (value, 0.82f);
                if (wet) value = juce::jmax (value, 0.80f);
                if (tone) value = juce::jmin (1.0f, value * 1.18f);
                if (level) value = juce::jlimit (0.0f, 1.0f, value * 0.90f);
                break;
        }

        return juce::jlimit (0.0f, 1.0f, value);
    }

    std::vector<Preset> makePresets() const
    {
        juce::StringArray names { "INIT", "CLEAN", "PUNCH", "WIDE", "EXTREME" };
        const auto category = upper (name);
        if (category.contains ("REVERB")) names = juce::StringArray { "INIT", "ROOM", "PLATE", "HALL", "ARENA" };
        else if (category.contains ("DELAY")) names = juce::StringArray { "INIT", "SLAP", "ECHO", "WIDE", "TAPE" };
        else if (category.contains ("CHORUS") || category.contains ("FLANGER") || category.contains ("PHASER") || category.contains ("VIBRATO")) names = juce::StringArray { "INIT", "CLASSIC", "MOTION", "WIDE", "JET" };
        else if (category.contains ("FUZZ") || category.contains ("DISTORT") || category.contains ("OVERDRIVE")) names = juce::StringArray { "INIT", "CRUNCH", "RHYTHM", "LEAD", "HEAVY" };
        else if (category.contains ("COMPRESS")) names = juce::StringArray { "INIT", "GLUE", "PUNCH", "SMOOTH", "LIMIT" };
        else if (category.contains ("EQ")) names = juce::StringArray { "INIT", "TIGHT", "BRIGHT", "PRESENCE", "SCULPT" };
        else if (category.contains ("FILTER")) names = juce::StringArray { "INIT", "FUNK", "QUACK", "SWEEP", "SYNTH" };
        else if (category.contains ("CLEANUP PRO")) names = juce::StringArray { "INIT", "VOCAL", "GUITAR", "PUNCH", "SURGICAL" };
        else if (category.contains ("TONE SCULPTOR")) names = juce::StringArray { "INIT", "WARM", "BRIGHT", "BODY", "MODERN" };
        else if (category.contains ("AMPSIM")) names = juce::StringArray { "INIT", "CLEAN", "CRUNCH", "LEAD", "MODERN" };

        std::vector<Preset> result;
        result.reserve ((size_t) names.size());

        for (int presetIndex = 0; presetIndex < names.size(); ++presetIndex)
        {
            Preset p;
            p.name = names[presetIndex];
            for (auto* parameter : processorRef.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
                if (ranged == nullptr)
                    continue;
                p.values.emplace_back (ranged->paramID,
                                       adjustForPreset (ranged->paramID,
                                                        defaultNormalised (*ranged),
                                                        presetIndex));
            }
            auto setPresetValue = [&] (const juce::String& id, float actualValue)
            {
                for (auto& [parameterId, normalised] : p.values)
                    if (parameterId == id)
                        if (auto* targetParameter = dynamic_cast<juce::RangedAudioParameter*> (findParameterById (id)))
                            normalised = targetParameter->getNormalisableRange().convertTo0to1 (actualValue);
            };

            if (category == "CLEANUP PRO" && presetIndex > 0)
            {
                static constexpr float values[4][4] = {
                    { -45.0f, 5500.0f, 0.35f, 0.25f },
                    { -50.0f, 4500.0f, 0.15f, 0.55f },
                    { -42.0f, 5000.0f, 0.20f, 0.80f },
                    { -38.0f, 6500.0f, 0.55f, 0.15f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("GATE", v[0]); setPresetValue ("DEESSF", v[1]);
                setPresetValue ("DEESSA", v[2]); setPresetValue ("TRANS", v[3]);
            }
            else if (category == "TONE SCULPTOR" && presetIndex > 0)
            {
                static constexpr float values[4][6] = {
                    { 2.0f, 4.0f, 3.0f, -1.0f, 1.0f, -1.0f },
                    { 3.0f, 7.0f, -1.0f, 3.0f, 1.0f, -1.0f },
                    { 4.0f, 4.5f, 5.0f, 0.0f, 1.0f, -1.0f },
                    { 6.0f, 6.5f, 1.0f, 4.0f, 0.90f, -2.0f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("DRIVE", v[0]); setPresetValue ("TONE", v[1]);
                setPresetValue ("BODY", v[2]); setPresetValue ("AIR", v[3]);
                setPresetValue ("MIX", v[4]); setPresetValue ("LEVEL", v[5]);
                if (auto* style = findParameterById ("STYLE"))
                    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (style))
                        for (auto& [id, normalised] : p.values)
                            if (id == "STYLE")
                                normalised = choice->convertTo0to1 (presetIndex - 1);
            }
            else if (category == "AMPSIM" && presetIndex > 0)
            {
                static constexpr float values[4][7] = {
                    { 1.5f, 0.0f, 5.0f, 5.5f, 5.5f, 4.5f, 6.0f },
                    { 4.0f, 1.0f, 5.5f, 6.0f, 5.0f, 5.5f, 6.0f },
                    { 6.5f, 3.0f, 5.0f, 4.0f, 6.5f, 6.5f, 5.5f },
                    { 8.0f, 4.0f, 4.5f, 5.0f, 7.0f, 7.0f, 5.0f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("GAIN", v[0]);
                if (auto* voice = findParameterById ("VOICE"))
                    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (voice))
                        for (auto& [id, normalised] : p.values) if (id == "VOICE") normalised = choice->convertTo0to1 (static_cast<float> (v[1]));
                setPresetValue ("BASS", v[2]); setPresetValue ("MID", v[3]); setPresetValue ("TREBLE", v[4]);
                setPresetValue ("PRESENCE", v[5]); setPresetValue ("LEVEL", v[6]);
            }

            result.push_back (std::move (p));
        }
        return result;
    }

    void applyPreset (const Preset& preset)
    {
        for (const auto& [id, normalised] : preset.values)
            if (auto* parameter = apvts.getParameter (id))
                parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    }

    juce::AudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String name;
    juce::Colour accent;

    juce::Label title, brand;
    juce::ComboBox presetBox;
    juce::TextButton instrumentButton;
    DevKomodoKnobLookAndFeel knobLookAndFeel;
    std::vector<Knob> knobs;
    std::vector<Choice> choices;
    std::vector<Toggle> toggles;
    std::vector<Preset> factoryPresets;
    std::vector<juce::Rectangle<int>> controlCellBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevKomodoUniversalEditor)
};
