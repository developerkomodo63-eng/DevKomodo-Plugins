#pragma once

#include <JuceHeader.h>
#include "../../Common/ParameterTooltips.h"
#include "../../Common/PresetProfiles.h"
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

    // Combo boxes (Mode/Style/Voice selectors like Tube, Tape, Diode, etc.)
    // used JUCE's default font here, which reads small and thin in these
    // compact panels. Bumping both the closed-box text and the dropdown
    // list itself makes the actual option names easy to read.
    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (15.0f, juce::Font::bold));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (16.0f, juce::Font::plain));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                        int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (18, 20, 25));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (box.isPopupActive() ? arcColour : juce::Colour::fromRGB (70, 74, 84));
        g.drawRoundedRectangle (bounds, 6.0f, 1.4f);

        auto arrowZone = bounds.removeFromRight (22.0f);
        juce::Path arrow;
        const float cx = arrowZone.getCentreX(), cy = arrowZone.getCentreY();
        arrow.addTriangle (cx - 5.0f, cy - 2.5f, cx + 5.0f, cy - 2.5f, cx, cy + 4.0f);
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
        : AudioProcessorEditor (&processor), processorRef (processor), apvts (state), name (std::move (productName)), accent (accentColour), knobLookAndFeel (accentColour), tooltipWindow (this, 650)
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
                                  || name.toUpperCase().contains ("MULTIBAND DRIVE")
                                  || name.toUpperCase().contains ("CONVOLUTION REVERB");
#if defined (DEVKOMODO_DEMO_BUILD)
        brand.setText ("DEVKOMODO  •  DEMO  •  15 MIN", juce::dontSendNotification);
#else
        brand.setText (premiumProduct ? "DEVKOMODO  •  PREMIUM" : "DEVKOMODO", juce::dontSendNotification);
#endif
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
            {
                applyPreset (factoryPresets[(size_t) id - 2]);
                presetDescription.setText (describePreset (factoryPresets[(size_t) id - 2].name), juce::dontSendNotification);
            }
            else
            {
                presetDescription.setText (describePreset ("MANUAL"), juce::dontSendNotification);
            }
        };
        presetBox.setTooltip ("Select a factory preset or MANUAL");
        presetBox.setLookAndFeel (&knobLookAndFeel);
        addAndMakeVisible (presetBox);
        presetDescription.setText (describePreset ("MANUAL"), juce::dontSendNotification);
        presetDescription.setFont (juce::Font (juce::FontOptions (10.0f)));
        presetDescription.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.58f));
        presetDescription.setTooltip ("Brief description of the selected factory preset");
        addAndMakeVisible (presetDescription);

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
                c.box->setTooltip (devkomodo::parameterTooltip (id, ranged->name));
                c.box->setLookAndFeel (&knobLookAndFeel);
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
                t.button->setTooltip (devkomodo::parameterTooltip (id, ranged->name));
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
                k.slider->setTooltip (devkomodo::parameterTooltip (id, ranged->name));
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
        for (auto& c : choices)
            if (c.box != nullptr) c.box->setLookAndFeel (nullptr);
        presetBox.setLookAndFeel (nullptr);
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

        auto presetInfo = footer.removeFromLeft (juce::jmin (300, footer.getWidth() / 2));
        presetDescription.setBounds (presetInfo.reduced (3, 2));

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


    static juce::String describePreset (const juce::String& presetName)
    {
        const auto preset = presetName.toUpperCase();
        if (preset == "MANUAL") return "MANUAL - edit the controls freely";
        if (preset == "INIT") return "INIT - neutral starting point";
        if (preset == "CLEAN") return "CLEAN - controlled dynamics and subtle colour";
        if (preset == "PUNCH") return "PUNCH - tighter attack and forward mids";
        if (preset == "WIDE") return "WIDE - broader movement and space";
        if (preset == "EXTREME") return "EXTREME - maximum character and intensity";
        if (preset == "CRUNCH") return "CRUNCH - responsive edge-of-breakup drive";
        if (preset == "RHYTHM") return "RHYTHM - focused, mix-ready rhythm tone";
        if (preset == "LEAD") return "LEAD - sustain and upper-mid presence";
        if (preset == "HEAVY") return "HEAVY - dense high-gain character";
        if (preset == "ROOM") return "ROOM - short, natural ambience";
        if (preset == "PLATE") return "PLATE - bright, smooth sustain";
        if (preset == "HALL") return "HALL - spacious decay for open parts";
        if (preset == "ARENA") return "ARENA - long, dramatic space";
        return presetName + " - factory starting point";
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
        // "FREQ" (not just the full word "FREQUENCY") so abbreviated IDs like
        // MINFREQ/MAXFREQ/ROTATIONFREQ are covered too -- those were
        // previously frozen at their default in every preset.
        const bool tone = key.contains ("TONE") || key.contains ("TREBLE") || key.contains ("HIGH")
                       || key.contains ("PRESENCE") || key.contains ("FREQ");
        const bool level = key.contains ("LEVEL") || key.contains ("OUTPUT") || key.contains ("MAKEUP");
        // "THRESH" (not the full word "THRESHOLD") so abbreviated IDs like
        // Clipper/Limiter's THRESH are covered too.
        const bool gate = key.contains ("GATE") || key.contains ("THRESH");
        // RATE/SPEED is the single most audible control on almost every
        // modulation pedal (Chorus/Flanger/Phaser/Tremolo/Vibrato/Doubler/
        // RotarySpeaker), and previously matched no bucket at all, so those
        // pedals' presets differed only in mix/feedback/depth while the
        // actual motion speed never changed.
        const bool motion = key.contains ("RATE") || key.contains ("SPEED");

        switch (preset)
        {
            case 0: // Factory Reset: defaults.
                break;
            case 1: // Clean / controlled.
                if (drive) value *= 0.45f;
                if (wet) value = juce::jmap (value, 0.0f, 1.0f, 0.15f, 0.45f);
                if (tone) value = juce::jmap (value, 0.0f, 1.0f, 0.42f, 0.62f);
                if (level) value = juce::jmap (value, 0.0f, 1.0f, 0.45f, 0.58f);
                if (motion) value = juce::jmap (value, 0.0f, 1.0f, 0.20f, 0.38f);
                break;
            case 2: // Punch.
                if (drive) value = juce::jmax (value, 0.65f);
                if (wet) value = juce::jlimit (0.0f, 1.0f, value * 1.15f);
                if (time) value *= 0.75f;
                if (gate) value = juce::jlimit (0.0f, 1.0f, value * 0.85f);
                if (motion) value = juce::jmap (value, 0.0f, 1.0f, 0.40f, 0.55f);
                break;
            case 3: // Wide / ambient.
                if (wet) value = juce::jmax (value, 0.72f);
                if (time) value = juce::jmax (value, 0.55f);
                if (tone) value = juce::jmin (1.0f, value * 1.10f);
                if (motion) value = juce::jmap (value, 0.0f, 1.0f, 0.25f, 0.42f);
                break;
            default: // Extreme / modern.
                if (drive) value = juce::jmax (value, 0.82f);
                if (wet) value = juce::jmax (value, 0.80f);
                if (tone) value = juce::jmin (1.0f, value * 1.18f);
                if (level) value = juce::jlimit (0.0f, 1.0f, value * 0.90f);
                if (motion) value = juce::jmap (value, 0.0f, 1.0f, 0.65f, 0.90f);
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
        else if (category.contains ("MULTIBAND DRIVE")) names = juce::StringArray { "INIT", "AIR", "PRESENCE", "GRIT", "CRUSH" };
        else if (category.contains ("DRUM ENHANCER")) names = juce::StringArray { "INIT", "TIGHT KICK", "TRAP", "ROCK", "SMASH" };
        else if (category.contains ("BASS ENHANCER")) names = juce::StringArray { "INIT", "SMALL SPEAKER", "STUDIO", "PICK CLARITY", "MODERN" };
        else if (category.contains ("GUITAR ENHANCER")) names = juce::StringArray { "INIT", "MIX CLARITY", "STRUM CONTROL", "LEAD CUT", "LIVE" };
        else if (category.contains ("KEYS ENHANCER")) names = juce::StringArray { "INIT", "WIDE PAD", "TIGHT COMP", "AIRY", "LUSH" };
        else if (category.contains ("ACOUSTIC GUITAR ENHANCER")) names = juce::StringArray { "INIT", "STUDIO", "LIVE STRUM", "FINGERSTYLE", "BRIGHT" };
        else if (category.contains ("VOCAL ENHANCER")) names = juce::StringArray { "INIT", "PODCAST", "LEAD VOCAL", "BREATHY", "BROADCAST" };
        else if (category.contains ("CASSETTE EMULATION")) names = juce::StringArray { "INIT", "MIXTAPE", "LO-FI BOOMBOX", "WALKMAN", "DEMO 4-TRACK" };

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
                setPresetValue ("MIX", 1.0f); setPresetValue ("OUTPUT", 0.0f);
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
            else if (category == "BITCRUSHER" && presetIndex > 0)
            {
                // BITS/RATEDIV are the entire identity of this pedal and
                // previously never moved between presets.
                static constexpr float values[4][2] = {
                    { 12.0f, 2.0f }, { 8.0f, 4.0f }, { 10.0f, 3.0f }, { 4.0f, 12.0f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("BITS", v[0]); setPresetValue ("RATEDIV", v[1]);
            }
            else if (category == "GRANULATOR" && presetIndex > 0)
            {
                static constexpr float values[4][4] = {
                    { 150.0f, 8.0f, 0.0f, 0.10f },
                    { 60.0f, 25.0f, 2.0f, 0.30f },
                    { 200.0f, 15.0f, 5.0f, 0.50f },
                    { 25.0f, 45.0f, 9.0f, 0.90f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("GRAINSIZE", v[0]); setPresetValue ("DENSITY", v[1]);
                setPresetValue ("PITCHSPREAD", v[2]); setPresetValue ("POSITIONJITTER", v[3]);
            }
            else if (category == "TRANSIENT SHAPER" && presetIndex > 0)
            {
                static constexpr float values[4][3] = {
                    { 2.0f, -1.0f, 1.5f }, { 8.0f, -3.0f, 2.5f },
                    { 3.0f, 4.0f, 1.8f }, { 12.0f, -8.0f, 3.5f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("ATTACK", v[0]); setPresetValue ("SUSTAIN", v[1]); setPresetValue ("SENSITIVITY", v[2]);
            }
            else if (category == "VINYL EMULATION" && presetIndex > 0)
            {
                static constexpr float values[4][3] = {
                    { 0.10f, 0.08f, 0.15f }, { 0.30f, 0.25f, 0.35f },
                    { 0.45f, 0.20f, 0.40f }, { 0.75f, 0.70f, 0.85f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("WOW", v[0]); setPresetValue ("CRACKLE", v[1]); setPresetValue ("AGE", v[2]);
            }
            else if (category == "OCTAVER" && presetIndex > 0)
            {
                static constexpr float values[4][4] = {
                    { 0.4f, 0.0f, 0.0f, 0.9f }, { 0.7f, 0.2f, 0.1f, 0.7f },
                    { 0.5f, 0.3f, 0.4f, 0.6f }, { 0.8f, 0.7f, 0.6f, 0.4f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("SUB1", v[0]); setPresetValue ("SUB2", v[1]);
                setPresetValue ("UP", v[2]); setPresetValue ("DRY", v[3]);
            }
            else if (category == "REVERB" && presetIndex > 0)
            {
                // SIZE/DAMPING/SHIMMER define room/plate/hall/arena far more
                // than MIX/WIDTH alone did.
                static constexpr float values[4][4] = {
                    { 0.25f, 0.60f, 0.22f, 0.0f }, { 0.45f, 0.35f, 0.30f, 0.0f },
                    { 0.75f, 0.40f, 0.35f, 0.10f }, { 0.95f, 0.25f, 0.45f, 0.30f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("SIZE", v[0]); setPresetValue ("DAMPING", v[1]);
                setPresetValue ("MIX", v[2]); setPresetValue ("SHIMMER", v[3]);
            }
            else if (category == "BROADCAST COMPRESSOR" && presetIndex > 0)
            {
                static constexpr float values[4] = { 3.0f, 6.0f, 4.5f, 9.0f };
                setPresetValue ("COMPRESSION", values[(size_t) juce::jlimit (0, 3, presetIndex - 1)]);
            }
            else if (category == "EQ 6-BAND" && presetIndex > 0)
            {
                // The generic FREQ-bucket heuristic (added for abbreviated
                // IDs like MINFREQ) was pulling all 6 band FREQ params
                // toward the same normalised range, collapsing this pedal's
                // deliberately spaced band centers together. Explicitly
                // pinning every *_FREQ back to its default and only shaping
                // gain per band avoids that.
                static constexpr float gains[4][6] = {
                    { -3.0f, -2.0f, 0.0f, 1.0f, 2.0f, 0.0f },   // Tight
                    { -1.0f, 0.0f, 0.0f, 2.0f, 4.0f, 3.0f },    // Bright
                    { 0.0f, 0.0f, 1.0f, 4.0f, 2.0f, 1.0f },     // Presence
                    { 3.0f, 1.0f, -4.0f, -2.0f, 2.0f, 3.0f }    // Sculpt
                };
                static constexpr float levels[4] = { 0.5f, 0.0f, 0.0f, 1.0f };
                static constexpr float freqDefaults[6] = { 80.0f, 250.0f, 800.0f, 2000.0f, 4000.0f, 8000.0f };
                const int idx = juce::jlimit (0, 3, presetIndex - 1);
                for (int b = 0; b < 6; ++b)
                {
                    setPresetValue ("BAND" + juce::String (b) + "_FREQ", freqDefaults[b]);
                    setPresetValue ("BAND" + juce::String (b) + "_GAIN", gains[idx][b]);
                }
                setPresetValue ("LEVEL", levels[idx]);
            }
            else if (category == "COMPRESSOR" && presetIndex > 0)
            {
                // The 4th preset is literally named "LIMIT" -- make sure it
                // actually switches into the new Limiter mode, not just a
                // higher ratio while staying in Compressor mode.
                static constexpr float threshold[4] = { -18.0f, -14.0f, -22.0f, -10.0f };
                static constexpr float ratio[4]     = { 3.0f, 5.0f, 2.5f, 4.0f };
                static constexpr float attack[4]    = { 15.0f, 5.0f, 25.0f, 0.5f };
                static constexpr float release[4]   = { 150.0f, 90.0f, 220.0f, 60.0f };
                static constexpr float makeup[4]    = { 4.0f, 6.0f, 3.0f, 8.0f };
                static constexpr float mode[4]      = { 0.0f, 0.0f, 0.0f, 1.0f }; // Limit -> Limiter mode
                const int idx = juce::jlimit (0, 3, presetIndex - 1);
                setPresetValue ("THRESHOLD", threshold[idx]); setPresetValue ("RATIO", ratio[idx]);
                setPresetValue ("ATTACK", attack[idx]); setPresetValue ("RELEASE", release[idx]);
                setPresetValue ("MAKEUP", makeup[idx]); setPresetValue ("MODE", mode[idx]);
            }
            else if (category == "BASS ENHANCER" && presetIndex > 0)
            {
                // focus, harmonics, tight, mix, level
                static constexpr float values[4][5] = {
                    { 150.0f, 7.0f, 3.0f, 0.75f, 1.0f },  // Small Speaker: max harmonics for translation
                    { 100.0f, 3.0f, 2.0f, 0.50f, 0.0f },  // Studio: subtle, trust the monitors
                    { 130.0f, 4.0f, 7.0f, 0.65f, 0.0f },  // Pick Clarity: tightens up between notes fast
                    { 160.0f, 6.0f, 5.0f, 0.70f, 1.0f }   // Modern: pushed across the board
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("FOCUS", v[0]); setPresetValue ("HARMONICS", v[1]);
                setPresetValue ("TIGHT", v[2]); setPresetValue ("MIX", v[3]); setPresetValue ("LEVEL", v[4]);
            }
            else if (category == "GUITAR ENHANCER" && presetIndex > 0)
            {
                // clarity, presence, mix, level
                static constexpr float values[4][4] = {
                    { 7.0f, 3.0f, 0.80f, 0.0f },  // Mix Clarity: dig the mud out
                    { 8.0f, 2.0f, 0.85f, -1.0f }, // Strum Control: heavy adaptive cut for dense chords
                    { 3.0f, 7.0f, 0.70f, 0.0f },  // Lead Cut: presence-forward for single-note lines
                    { 5.0f, 5.0f, 0.75f, 0.5f }   // Live: balanced, a bit more level for the room
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("CLARITY", v[0]); setPresetValue ("PRESENCE", v[1]);
                setPresetValue ("MIX", v[2]); setPresetValue ("LEVEL", v[3]);
            }
            else if (category == "KEYS ENHANCER" && presetIndex > 0)
            {
                // shimmer, width, mix, level
                static constexpr float values[4][4] = {
                    { 3.0f, 8.0f, 0.85f, 0.0f },  // Wide Pad: width-forward for sustained pads
                    { 2.0f, 2.0f, 0.60f, 0.0f },  // Tight Comp: minimal, keeps things centered/mono-safe
                    { 8.0f, 4.0f, 0.75f, 0.5f },  // Airy: shimmer-forward
                    { 6.0f, 6.0f, 0.80f, 0.0f }   // Lush: both pushed
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("SHIMMER", v[0]); setPresetValue ("WIDTH", v[1]);
                setPresetValue ("MIX", v[2]); setPresetValue ("LEVEL", v[3]);
            }
            else if (category == "ACOUSTIC GUITAR ENHANCER" && presetIndex > 0)
            {
                // debox, sparkle, mix, level
                static constexpr float values[4][4] = {
                    { 4.0f, 3.0f, 0.65f, 0.0f },  // Studio: gentle, already well-mic'd
                    { 8.0f, 4.0f, 0.80f, 0.5f },  // Live Strum: heavy debox for a boomy stage mic
                    { 3.0f, 6.0f, 0.70f, 0.0f },  // Fingerstyle: sparkle-forward, minimal debox
                    { 5.0f, 8.0f, 0.75f, 0.5f }   // Bright: both pushed
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("DEBOX", v[0]); setPresetValue ("SPARKLE", v[1]);
                setPresetValue ("MIX", v[2]); setPresetValue ("LEVEL", v[3]);
            }
            else if (category == "CASSETTE EMULATION" && presetIndex > 0)
            {
                // type, wow, dolby, hiss, mix, level
                static constexpr float values[4][6] = {
                    { 1.0f, 3.0f, 4.0f, 2.0f, 0.85f, 0.0f },  // Mixtape: chrome, clean-ish but present
                    { 0.0f, 7.0f, 6.0f, 7.0f, 1.0f, -1.0f },  // Lo-Fi Boombox: ferric, worn out
                    { 1.0f, 5.0f, 5.0f, 4.0f, 0.90f, 0.0f },  // Walkman: chrome, moderate wobble
                    { 0.0f, 8.0f, 3.0f, 8.0f, 1.0f, -2.0f }   // Demo 4-Track: ferric, hissy and loose
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("TYPE", v[0]); setPresetValue ("WOW", v[1]);
                setPresetValue ("DOLBY", v[2]); setPresetValue ("HISS", v[3]);
                setPresetValue ("MIX", v[4]); setPresetValue ("LEVEL", v[5]);
            }
            else if (category == "VOCAL ENHANCER" && presetIndex > 0)
            {
                // warmth, air, mix, level
                static constexpr float values[4][4] = {
                    { 5.0f, 4.0f, 0.75f, 0.0f },  // Podcast: fuller voiced content, controlled air
                    { 4.0f, 5.0f, 0.70f, 0.0f },  // Lead Vocal: balanced
                    { 2.0f, 8.0f, 0.80f, 0.5f },  // Breathy: air-forward, minimal warmth
                    { 6.0f, 3.0f, 0.65f, 0.0f }   // Broadcast: warm, controlled top end
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("WARMTH", v[0]); setPresetValue ("AIR", v[1]);
                setPresetValue ("MIX", v[2]); setPresetValue ("LEVEL", v[3]);
            }
            else if (category == "DRUM ENHANCER" && presetIndex > 0)
            {
                // crossover, punch, sub, crack, mix, level
                static constexpr float values[4][6] = {
                    { 500.0f, 3.0f, 5.0f, 1.0f, 0.55f, 0.0f },  // Tight Kick: sub-forward, minimal top
                    { 700.0f, 5.0f, 2.0f, 6.0f, 0.65f, 0.5f },  // Trap: crack-forward 808-style click
                    { 550.0f, 6.0f, 4.0f, 5.0f, 0.60f, 0.0f },  // Rock: balanced punch across the kit
                    { 450.0f, 8.0f, 7.0f, 8.0f, 0.80f, -1.0f }  // Smash: everything pushed hard
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("CROSSOVER", v[0]); setPresetValue ("PUNCH", v[1]);
                setPresetValue ("SUB", v[2]); setPresetValue ("CRACK", v[3]);
                setPresetValue ("MIX", v[4]); setPresetValue ("LEVEL", v[5]);
            }
            else if (category == "MULTIBAND DRIVE" && presetIndex > 0)
            {
                // xoverLow, xoverHigh, lowDrive, midDrive, highDrive, level
                static constexpr float values[4][6] = {
                    { 500.0f, 3500.0f, 0.0f, 0.0f, 2.5f, 0.0f },  // Air: gentle high-band air
                    { 350.0f, 2800.0f, 0.0f, 1.5f, 4.0f, 0.5f },  // Presence: mid+high forward
                    { 300.0f, 2200.0f, 2.0f, 3.0f, 5.5f, -1.0f }, // Grit: all three bands driven
                    { 250.0f, 1800.0f, 4.0f, 6.0f, 8.0f, -3.0f }  // Crush: heavy across the board
                };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                setPresetValue ("XOVERLOW", v[0]); setPresetValue ("XOVERHIGH", v[1]);
                setPresetValue ("LOWDRIVE", v[2]); setPresetValue ("MIDDRIVE", v[3]);
                setPresetValue ("HIGHDRIVE", v[4]); setPresetValue ("LEVEL", v[5]);
            }
            else if (category == "GRAPHIC EQ" && presetIndex > 0)
            {
                // 10 ISO bands: 31/62/125/250/500/1k/2k/4k/8k/16k Hz.
                static constexpr float values[4][10] = {
                    { -3.0f, -3.0f, -2.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
                    { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 3.0f, 4.0f, 3.0f },
                    { 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 3.0f, 4.0f, 2.0f, 1.0f },
                    { 2.0f, 1.0f, 0.0f, -2.0f, -3.0f, -2.0f, 0.0f, 2.0f, 3.0f, 3.0f } };
                const auto& v = values[(size_t) juce::jlimit (0, 3, presetIndex - 1)];
                for (int b = 0; b < 10; ++b)
                    setPresetValue ("BAND" + juce::String (b), v[b]);
            }

            for (const auto& [id, value] : devkomodo::curatedPresetValues (category, presetIndex))
                setPresetValue (id, value);

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

    juce::Label title, brand, presetDescription;
    juce::ComboBox presetBox;
    juce::TextButton instrumentButton;
    DevKomodoKnobLookAndFeel knobLookAndFeel;
    juce::TooltipWindow tooltipWindow;
    std::vector<Knob> knobs;
    std::vector<Choice> choices;
    std::vector<Toggle> toggles;
    std::vector<Preset> factoryPresets;
    std::vector<juce::Rectangle<int>> controlCellBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevKomodoUniversalEditor)
};
