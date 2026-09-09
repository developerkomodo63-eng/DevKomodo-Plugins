#pragma once

#include <JuceHeader.h>
#include "../../Common/ParameterTooltips.h"
#include "../../Common/TempoSync.h"
#include "DevKomodoUI.h"
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <utility>
#include <array>

// ============================================================================
// Bespoke "hardware panel" editor for Juno Emu.
//
// The rest of the DevKomodo range shares one generic knob-grid editor
// (DevKomodoUI.h) that auto-lays-out every parameter as a rotary knob. Juno
// Emu is a DCO/VCF/VCA synth voice, not an effect pedal, so it gets its own
// editor here: vertical "slider panel" controls in the classic Juno-106
// layout (LFO -> DCO -> HPF/VCF -> ENV -> CHORUS), LED-style pill buttons for
// the wave/octave/chorus mode choices instead of drop-downs, and three small
// live visualizers (DCO scope, VCA envelope shape, VCF response curve) so the
// panel actually shows what the sound is doing rather than being knobs only.
// ============================================================================

namespace junoui
{
    // A rectangular "hardware slider" handle on a dark track, in the spirit
    // of the real Juno-106's linear sliders -- used only for this editor's
    // classic-section controls so it reads differently from every other
    // DevKomodo pedal's rotary knobs.
    class JunoSliderLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        explicit JunoSliderLookAndFeel (juce::Colour accentColour) : accent (accentColour) {}

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                const juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) override
        {
            auto track = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced ((float) width * 0.5f - 3.0f, 6.0f);

            g.setColour (juce::Colour::fromRGB (9, 8, 7));
            g.fillRoundedRectangle (track, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (track, 3.0f, 1.0f);

            auto filled = track.withTop (juce::jlimit (track.getY(), track.getBottom(), sliderPos));
            g.setColour (accent.withAlpha (0.55f));
            g.fillRoundedRectangle (filled, 3.0f);

            const float handleH = 11.0f;
            auto handle = juce::Rectangle<float> ((float) x + 1.0f, sliderPos - handleH * 0.5f,
                                                   (float) width - 2.0f, handleH);
            g.setColour (juce::Colours::white.withAlpha (0.94f));
            g.fillRoundedRectangle (handle, 2.0f);
            g.setColour (accent);
            g.drawRoundedRectangle (handle, 2.0f, 1.3f);
            g.fillRect (juce::Rectangle<float> (handle.getX() + 3.0f, handle.getCentreY() - 0.75f,
                                                 handle.getWidth() - 6.0f, 1.5f));
        }

        juce::Font getLabelFont (juce::Label&) override
        {
            return juce::Font (juce::FontOptions (9.5f, juce::Font::bold));
        }

    private:
        juce::Colour accent;
    };

    //--------------------------------------------------------------------
    // Small rounded "LED" pill button, used to build mode-selector rows
    // (wave shape, sub octave, chorus mode) instead of combo boxes.
    //--------------------------------------------------------------------
    class PillLedButton final : public juce::Button
    {
    public:
        PillLedButton (const juce::String& buttonText, juce::Colour accentColour)
            : juce::Button (buttonText), text (buttonText), accent (accentColour) {}

        void paintButton (juce::Graphics& g, bool isOver, bool /*isDown*/) override
        {
            auto b = getLocalBounds().toFloat().reduced (1.0f);
            const bool on = getToggleState();

            g.setColour (on ? accent.withAlpha (0.22f) : juce::Colour::fromRGB (15, 13, 11));
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
            g.setColour (on ? accent : juce::Colours::white.withAlpha (isOver ? 0.32f : 0.16f));
            g.drawRoundedRectangle (b.reduced (0.5f), b.getHeight() * 0.5f, 1.2f);

            auto dot = juce::Rectangle<float> (b.getX() + 6.0f, b.getCentreY() - 2.8f, 5.6f, 5.6f);
            g.setColour (on ? accent : juce::Colours::white.withAlpha (0.18f));
            g.fillEllipse (dot);

            g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.55f));
            g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
            g.drawText (text, b.withTrimmedLeft (15.0f), juce::Justification::centredLeft);
        }

    private:
        juce::String text;
        juce::Colour accent;
    };

    //--------------------------------------------------------------------
    // PanelSection hands every item the full column height (knobs and the
    // pill-button stacks want that); a native juce::ComboBox doesn't --
    // it looks stretched and strange filled out to 80+ px tall. This just
    // centres a normal-height dropdown in whatever space it's given.
    //--------------------------------------------------------------------
    class DropdownField final : public juce::Component
    {
    public:
        explicit DropdownField (juce::ComboBox& boxToHost) : box (boxToHost)
        {
            addAndMakeVisible (box);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            box.setBounds (b.withSizeKeepingCentre (b.getWidth(), juce::jmin (24, b.getHeight())));
        }

    private:
        juce::ComboBox& box;
    };

    //--------------------------------------------------------------------
    // A stack of PillLedButtons bound directly to an AudioParameterChoice.
    //--------------------------------------------------------------------
    class ChoiceSelector final : public juce::Component
    {
    public:
        ChoiceSelector (juce::AudioProcessorValueTreeState& state, juce::String paramID,
                         juce::StringArray optionLabels, juce::Colour accentColour,
                         const juce::String& tooltipText = {})
            : apvts (state), id (std::move (paramID)), labels (std::move (optionLabels))
        {
            for (int i = 0; i < labels.size(); ++i)
            {
                auto* btn = buttons.add (new PillLedButton (labels[i], accentColour));
                btn->onClick = [this, i] { select (i); };
                if (tooltipText.isNotEmpty())
                    btn->setTooltip (tooltipText);
                addAndMakeVisible (btn);
            }
            if (auto* p = apvts.getParameter (id))
                current = juce::roundToInt (p->getValue() * (float) juce::jmax (1, labels.size() - 1));
            refresh();
        }

        void refreshFromParameter()
        {
            if (auto* p = apvts.getParameter (id))
            {
                const int idx = juce::roundToInt (p->getValue() * (float) juce::jmax (1, labels.size() - 1));
                if (idx != current) { current = idx; refresh(); }
            }
        }

        void resized() override
        {
            auto b = getLocalBounds();
            const int n = juce::jmax (1, buttons.size());
            const int h = b.getHeight() / n;
            int y = b.getY();
            for (auto* btn : buttons)
            {
                btn->setBounds (b.getX(), y, b.getWidth(), h - 3);
                y += h;
            }
        }

    private:
        void select (int index)
        {
            current = index;
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost ((float) index / (float) juce::jmax (1, labels.size() - 1));
            refresh();
        }

        void refresh()
        {
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState (i == current, juce::dontSendNotification);
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String id;
        juce::StringArray labels;
        juce::OwnedArray<PillLedButton> buttons;
        int current = 0;
    };

    //--------------------------------------------------------------------
    // Horizontal wave tabs. The oscillator waveform is a mode choice, not a
    // column of permanently visible buttons; this keeps the synth compact
    // and gives the oscillator area the modern, Serum-like workflow.
    //--------------------------------------------------------------------
    class WaveTabSelector final : public juce::Component
    {
    public:
        WaveTabSelector (juce::AudioProcessorValueTreeState& state, juce::String paramID,
                         juce::StringArray optionLabels, juce::Colour accentColour,
                         const juce::String& tooltipText = {})
            : apvts (state), id (std::move (paramID)), labels (std::move (optionLabels)), accent (accentColour)
        {
            for (int i = 0; i < labels.size(); ++i)
            {
                auto* button = buttons.add (new juce::TextButton (labels[i]));
                button->setClickingTogglesState (false);
                button->onClick = [this, i] { select (i); };
                if (tooltipText.isNotEmpty())
                    button->setTooltip (tooltipText);
                addAndMakeVisible (button);
            }
            refresh();
        }

        void refreshFromParameter()
        {
            if (auto* p = apvts.getParameter (id))
            {
                const int idx = juce::roundToInt (p->getValue() * (float) juce::jmax (1, labels.size() - 1));
                if (idx != current) { current = idx; refresh(); }
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            const int gap = 3;
            const int n = juce::jmax (1, buttons.size());
            const int w = (area.getWidth() - gap * (n - 1)) / n;
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setBounds (area.getX() + i * (w + gap), area.getY(), w, area.getHeight());
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
        }

    private:
        void select (int index)
        {
            current = index;
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost ((float) index / (float) juce::jmax (1, labels.size() - 1));
            refresh();
        }

        void refresh()
        {
            for (int i = 0; i < buttons.size(); ++i)
            {
                auto* b = buttons[i];
                b->setColour (juce::TextButton::buttonColourId,
                              i == current ? accent.withAlpha (0.24f) : juce::Colour::fromRGB (16, 14, 12));
                b->setColour (juce::TextButton::textColourOffId,
                              i == current ? juce::Colours::white : juce::Colours::white.withAlpha (0.52f));
                b->setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.30f));
                b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            }
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::String id;
        juce::StringArray labels;
        juce::Colour accent;
        juce::OwnedArray<juce::TextButton> buttons;
        int current = 0;
    };

    //--------------------------------------------------------------------
    // Drawable modulation source. Up to 32 host-automatable points are kept
    // for preset/state compatibility, while the editor exposes a selectable
    // working resolution (4 / 8 / 16 / 32 points) so the grid stays compact.
    // Two independent destinations turn one curve into
    // a compact modulation source for cutoff, resonance, wavetable position,
    // FM, PWM, oscillator pitch or VCA level.
    //--------------------------------------------------------------------
    class DrawableLfoPanel final : public juce::Component, private juce::Timer
    {
    public:
        DrawableLfoPanel (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
            : apvts (state), accent (accentColour)
        {
            for (int i = 0; i < maxPoints; ++i)
                points[(size_t) i] = "LFO1_POINT_" + juce::String (i).paddedLeft ('0', 2);

            configureCombo (destA, "LFO1_DEST_A");
            configureCombo (destB, "LFO1_DEST_B");
            configureAmount (amountA, "LFO1_AMT_A");
            configureAmount (amountB, "LFO1_AMT_B");
            configureRate (rate, "LFO_RATE");
            configureAmount (smooth, "LFO1_SMOOTH");
            configurePointCount();

            addAndMakeVisible (destA);
            addAndMakeVisible (destB);
            addAndMakeVisible (amountA);
            addAndMakeVisible (amountB);
            addAndMakeVisible (rate);
            addAndMakeVisible (smooth);
            addAndMakeVisible (pointCount);

            // Every control here used to have no caption at all -- fine for
            // a knob whose name is printed on hardware silkscreen, not for a
            // software panel where the only clue was position.
            makeLabel (destALabel, "DEST A");
            makeLabel (amountALabel, "AMT A");
            makeLabel (destBLabel, "DEST B");
            makeLabel (amountBLabel, "AMT B");
            makeLabel (rateLabel, "RATE");
            makeLabel (smoothLabel, "SMOOTH");
            makeLabel (pointCountLabel, "POINTS");

            // BPM sync for the Mod Shape rate: when engaged, LFO_RATE is
            // ignored and the curve instead advances in lock-step with the
            // host tempo at the chosen note division (see TempoSync.h /
            // DevKomodoTempoSync::resolveHz, applied in renderNextBlock()).
            syncButton.setButtonText ("SYNC");
            syncButton.setTooltip ("Lock the Mod Shape rate to the host tempo instead of running free in Hz");
            syncButton.setClickingTogglesState (true);
            syncButton.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.55f));
            if (auto* p = apvts.getParameter ("TEMPOSYNC"))
                syncButton.setToggleState (p->getValue() > 0.5f, juce::dontSendNotification);
            syncButton.onClick = [this]
            {
                if (auto* p = apvts.getParameter ("TEMPOSYNC"))
                    p->setValueNotifyingHost (syncButton.getToggleState() ? 1.0f : 0.0f);
                rate.setEnabled (! syncButton.getToggleState());
                noteDiv.setEnabled (syncButton.getToggleState());
            };
            addAndMakeVisible (syncButton);

            noteDiv.addItemList (DevKomodoTempoSync::noteDivisionChoices(), 1);
            noteDiv.setTooltip ("Note division used when SYNC is on");
            if (auto* p = apvts.getParameter ("NOTEDIV"))
                noteDiv.setSelectedId (juce::roundToInt (p->getValue() * 13.0f) + 1, juce::dontSendNotification);
            noteDiv.onChange = [this]
            {
                if (auto* p = apvts.getParameter ("NOTEDIV"))
                    p->setValueNotifyingHost ((float) (noteDiv.getSelectedId() - 1) / 13.0f);
            };
            addAndMakeVisible (noteDiv);
            rate.setEnabled (! syncButton.getToggleState());
            noteDiv.setEnabled (syncButton.getToggleState());

            startTimerHz (15);
        }

        ~DrawableLfoPanel() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colour::fromRGB (22, 19, 16));
            g.fillRoundedRectangle (b, 9.0f);
            g.setColour (accent.withAlpha (0.30f));
            g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.0f);

            auto graph = graphBounds();

            // Compact shaper grid: only the active breakpoints become major
            // columns. This keeps 4/8 point shapes readable instead of filling
            // the editor with 32 tiny guides.
            const int activePoints = getActivePointCount();
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (int i = 0; i < activePoints; ++i)
            {
                const float xNorm = (float) i / (float) (activePoints - 1);
                const float x = graph.getX() + xNorm * graph.getWidth();
                g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
            }
            g.setColour (juce::Colours::white.withAlpha (0.035f));
            for (int i = 1; i < activePoints - 1; ++i)
            {
                const float x = graph.getX() + graph.getWidth() * (float) i / (float) (activePoints - 1);
                g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
            }
            for (int i = 1; i < 4; ++i)
            {
                const float y = graph.getY() + graph.getHeight() * (float) i / 4.0f;
                g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
            }
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.drawHorizontalLine ((int) (graph.getY() + graph.getHeight() * 0.5f), graph.getX(), graph.getRight());

            juce::Path curve;
            for (int i = 0; i < 128; ++i)
            {
                const float xNorm = (float) i / 127.0f;
                const float value = sampleCurve (xNorm);
                const float x = graph.getX() + xNorm * graph.getWidth();
                const float y = graph.getBottom() - value * graph.getHeight();
                if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
            }
            g.setColour (accent);
            g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Draw only the selected working points. The unused backing
            // parameters remain hidden so presets/automation stay compatible.
            for (int i = 0; i < activePoints; ++i)
            {
                const float xNorm = (float) i / (float) (activePoints - 1);
                const float value = raw (pointIdForSlot (i, activePoints), xNorm);
                const float x = graph.getX() + xNorm * graph.getWidth();
                const float y = graph.getBottom() - value * graph.getHeight();
                const float r = 3.4f;
                g.setColour (juce::Colour::fromRGB (10, 9, 8));
                g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.3f);
            }

            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.setColour (juce::Colours::white.withAlpha (0.62f));
            g.drawText ("FILTER / MOD SHAPE", graph.getX() + 8, graph.getY() + 5, 130, 16, juce::Justification::left);
            g.drawText ("click a point, drag to shape", graph.getRight() - 150, graph.getY() + 5, 142, 16,
                        juce::Justification::right);
        }

        void mouseDown (const juce::MouseEvent& e) override { drawAt (e.position); }
        void mouseDrag (const juce::MouseEvent& e) override { drawAt (e.position); }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            auto syncRow = area.removeFromBottom (18);
            auto controls = area.removeFromBottom (42);
            auto labels = area.removeFromBottom (12);
            const int gap = 6;
            const int w = (controls.getWidth() - gap * 6) / 7;

            auto placeColumn = [&] (juce::Rectangle<int> labelArea, juce::Rectangle<int> controlArea,
                                     juce::Label& label, juce::Component& control)
            {
                label.setBounds (labelArea);
                control.setBounds (controlArea);
            };

            placeColumn (labels.removeFromLeft (w), controls.removeFromLeft (w), destALabel, destA);
            labels.removeFromLeft (gap); controls.removeFromLeft (gap);
            placeColumn (labels.removeFromLeft (w), controls.removeFromLeft (w), amountALabel, amountA);
            labels.removeFromLeft (gap); controls.removeFromLeft (gap);
            placeColumn (labels.removeFromLeft (w), controls.removeFromLeft (w), destBLabel, destB);
            labels.removeFromLeft (gap); controls.removeFromLeft (gap);
            placeColumn (labels.removeFromLeft (w), controls.removeFromLeft (w), amountBLabel, amountB);
            labels.removeFromLeft (gap); controls.removeFromLeft (gap);

            auto rateLabelArea = labels.removeFromLeft (w);
            auto rateControlArea = controls.removeFromLeft (w);
            labels.removeFromLeft (gap); controls.removeFromLeft (gap);
            placeColumn (rateLabelArea, rateControlArea, rateLabel, rate);

            placeColumn (labels.removeFromLeft (w), controls.removeFromLeft (w), smoothLabel, smooth);
            labels.removeFromLeft (gap);
            controls.removeFromLeft (gap);
            placeColumn (labels, controls, pointCountLabel, pointCount);

            // Sync row sits only under the RATE column -- SYNC toggle and
            // note-division combo, since they're specifically what makes
            // the Mod Shape rate follow the host tempo.
            auto syncArea = juce::Rectangle<int> (rateControlArea.getX(), syncRow.getY(),
                                                   juce::jmin (rateControlArea.getWidth() * 2 + gap,
                                                               syncRow.getRight() - rateControlArea.getX()),
                                                   syncRow.getHeight());
            syncButton.setBounds (syncArea.removeFromLeft (syncArea.getWidth() / 2 - 3));
            syncArea.removeFromLeft (6);
            noteDiv.setBounds (syncArea);
        }

    private:
        juce::Rectangle<int> graphBounds() const
        {
            return getLocalBounds().reduced (8).withTrimmedBottom (72);
        }

        float raw (const juce::String& id, float fallback) const
        {
            if (auto* p = apvts.getRawParameterValue (id)) return p->load();
            return fallback;
        }

        int getActivePointCount() const
        {
            static constexpr int pointCounts[] { 4, 8, 16, 32 };
            if (auto* p = apvts.getRawParameterValue ("LFO1_POINT_COUNT"))
            {
                const int index = juce::jlimit (0, 3, juce::roundToInt (p->load() * 3.0f));
                return pointCounts[index];
            }
            return 8;
        }

        const juce::String& pointIdForSlot (int slot, int activeCount) const
        {
            const int sourceIndex = (int) std::round ((double) slot * (maxPoints - 1) / (double) (activeCount - 1));
            return points[(size_t) juce::jlimit (0, maxPoints - 1, sourceIndex)];
        }

        float sampleCurve (float x) const
        {
            const int activeCount = getActivePointCount();
            const float pos = juce::jlimit (0.0f, 0.9999f, x) * (float) (activeCount - 1);
            const int i = juce::jlimit (0, activeCount - 2, (int) pos);
            float t = pos - (float) i;
            const float smoothing = raw ("LFO1_SMOOTH", 0.18f);
            const float smoothT = t * t * (3.0f - 2.0f * t);
            t = t * (1.0f - smoothing) + smoothT * smoothing;
            const float a = raw (pointIdForSlot (i, activeCount), (float) i / (float) (activeCount - 1));
            const float b = raw (pointIdForSlot (i + 1, activeCount), (float) (i + 1) / (float) (activeCount - 1));
            return juce::jlimit (0.0f, 1.0f, a + (b - a) * t);
        }

        void drawAt (juce::Point<float> pos)
        {
            auto graph = graphBounds().toFloat();
            if (! graph.contains (pos)) return;
            const float x = juce::jlimit (0.0f, 0.9999f, (pos.x - graph.getX()) / graph.getWidth());
            const float y = juce::jlimit (0.0f, 1.0f, 1.0f - (pos.y - graph.getY()) / graph.getHeight());
            const int activeCount = getActivePointCount();
            const int slot = juce::jlimit (0, activeCount - 1, juce::roundToInt (x * (float) (activeCount - 1)));
            if (auto* p = apvts.getParameter (pointIdForSlot (slot, activeCount)))
                p->setValueNotifyingHost (y);
            repaint();
        }

        void configurePointCount()
        {
            pointCount.addItem ("4", 1);
            pointCount.addItem ("8", 2);
            pointCount.addItem ("16", 3);
            pointCount.addItem ("32", 4);
            pointCount.setTooltip ("Number of editable points in the Mod Shape grid");
            if (auto* p = apvts.getParameter ("LFO1_POINT_COUNT"))
                pointCount.setSelectedId (juce::jlimit (1, 4, juce::roundToInt (p->getValue() * 3.0f) + 1), juce::dontSendNotification);
            pointCount.onChange = [this]
            {
                if (auto* p = apvts.getParameter ("LFO1_POINT_COUNT"))
                    p->setValueNotifyingHost ((float) (pointCount.getSelectedId() - 1) / 3.0f);
                repaint();
            };
        }

        void configureCombo (juce::ComboBox& box, const juce::String& id)
        {
            box.addItemList ({ "OFF", "CUTOFF", "RESONANCE", "WT POS", "FM", "PWM", "OSC2 PITCH", "OSC1 PITCH", "VCA" }, 1);
            box.setTooltip ("Choose where the drawn LFO is routed");
            if (auto* p = apvts.getParameter (id))
                box.setSelectedId (juce::roundToInt (p->getValue() * 8.0f) + 1, juce::dontSendNotification);
            box.onChange = [&box, this, id]
            {
                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost ((float) (box.getSelectedId() - 1) / 8.0f);
            };
        }

        void configureAmount (juce::Slider& slider, const juce::String& id)
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setRange (-1.0, 1.0, 0.001);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
            slider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.30f));
            slider.setColour (juce::Slider::thumbColourId, accent);
            slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.75f));
            slider.setScrollWheelEnabled (false);
            if (auto* p = apvts.getParameter (id))
                slider.setValue (p->convertFrom0to1 (p->getValue()), juce::dontSendNotification);
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
        }

        void configureRate (juce::Slider& slider, const juce::String& id)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
            slider.setScrollWheelEnabled (false);
            if (auto* p = apvts.getParameter (id))
                slider.setValue (p->convertFrom0to1 (p->getValue()), juce::dontSendNotification);
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
        }

        void makeLabel (juce::Label& label, const juce::String& text)
        {
            label.setText (text, juce::dontSendNotification);
            label.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
        }

        void timerCallback() override
        {
            if (auto* p = apvts.getParameter ("TEMPOSYNC"))
            {
                const bool synced = p->getValue() > 0.5f;
                if (synced != syncButton.getToggleState())
                {
                    syncButton.setToggleState (synced, juce::dontSendNotification);
                    rate.setEnabled (! synced);
                    noteDiv.setEnabled (synced);
                }
            }

            if (auto* p = apvts.getRawParameterValue ("LFO1_POINT_COUNT"))
            {
                const int id = juce::jlimit (1, 4, juce::roundToInt (p->load() * 3.0f) + 1);
                if (pointCount.getSelectedId() != id)
                    pointCount.setSelectedId (id, juce::dontSendNotification);
            }
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent;
        static constexpr int maxPoints = 32;
        std::array<juce::String, maxPoints> points {};
        juce::ComboBox destA, destB, pointCount;
        juce::Slider amountA, amountB, rate, smooth;
        juce::Label destALabel, amountALabel, destBLabel, amountBLabel, rateLabel, smoothLabel, pointCountLabel;
        juce::TextButton syncButton;
        juce::ComboBox noteDiv;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    };

    //--------------------------------------------------------------------
    // Bordered, titled group box that lays its children out as evenly
    // spaced columns (label on top, control filling the rest) -- the
    // "sections" a real Juno-106 panel is silkscreened into (DCO, VCF...).
    //--------------------------------------------------------------------
    class PanelSection final : public juce::Component
    {
    public:
        PanelSection (juce::String sectionTitle, juce::Colour accentColour)
            : title (std::move (sectionTitle)), accent (accentColour) {}

        void addItem (juce::Component* label, juce::Component* control)
        {
            items.push_back ({ label, control });
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colour::fromRGB (20, 17, 15));
            g.fillRoundedRectangle (b, 7.0f);
            g.setColour (accent.withAlpha (0.42f));
            g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.1f);
            g.setColour (accent);
            g.fillRoundedRectangle (b.getX(), b.getY(), 3.0f, b.getHeight(), 1.5f);

            g.setColour (accent.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (title, getLocalBounds().removeFromTop (16).withTrimmedLeft (9),
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            b.removeFromTop (17);
            b = b.reduced (5, 5);
            if (items.empty())
                return;
            const int n = (int) items.size();
            const int colW = juce::jmax (1, b.getWidth() / n);
            int x = b.getX();
            for (auto& it : items)
            {
                juce::Rectangle<int> col (x, b.getY(), colW, b.getHeight());
                if (it.label != nullptr)
                    it.label->setBounds (col.removeFromTop (12));
                it.control->setBounds (col.reduced (1, 0));
                x += colW;
            }
        }

    private:
        struct Item { juce::Component* label; juce::Component* control; };
        juce::String title;
        juce::Colour accent;
        std::vector<Item> items;
    };

    //--------------------------------------------------------------------
    // Animated oscilloscope-style view of the current DCO waveform, plus a
    // small orbiting dot that shows the LFO phase/rate/depth. Redrawn on a
    // timer -- it's a stylised readout (not tapped from the audio thread),
    // but it reacts live to WAVE / PULSE / SUB / NOISE / LFO parameters.
    //--------------------------------------------------------------------
    class WaveScopeDisplay final : public juce::Component, private juce::Timer
    {
    public:
        WaveScopeDisplay (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
            : apvts (state), accent (accentColour)
        {
            startTimerHz (30);
        }

        ~WaveScopeDisplay() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colour::fromRGB (9, 8, 7));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (accent.withAlpha (0.28f));
            g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

            g.setColour (accent.withAlpha (0.08f));
            for (int i = 1; i < 4; ++i)
            {
                const float y = b.getY() + b.getHeight() * (float) i / 4.0f;
                g.drawHorizontalLine ((int) y, b.getX() + 8.0f, b.getRight() - 8.0f);
            }

            auto scope = b.reduced (10.0f, 20.0f);
            const float waveIndex = raw ("WAVE", 2.0f);
            const float pulseWidth = raw ("PULSE", 0.5f);
            const float subLevel = raw ("SUB", 0.35f);
            const float noiseLevel = raw ("NOISE", 0.04f);
            const float osc2Wave = raw ("OSC2_WAVE", 0.0f);
            const float osc2Semi = raw ("OSC2_SEMI", 0.0f);
            const float osc2Fine = raw ("OSC2_FINE", 0.0f);
            const float osc2Level = raw ("OSC2_LEVEL", 0.0f);
            const float osc2Ratio = std::pow (2.0f, (osc2Semi + osc2Fine * 0.01f) / 12.0f);

            juce::Path path;
            juce::Path path2;
            constexpr int numPoints = 220;
            for (int i = 0; i <= numPoints; ++i)
            {
                const float t = (float) i / (float) numPoints;
                const float ph = std::fmod (t * 2.0f + phase, 1.0f);
                float sample = shapeSample (waveIndex, ph, pulseWidth, true);

                sample += subLevel * 0.4f * std::sin (juce::MathConstants<float>::twoPi * ph * 0.5f);
                sample += noiseLevel * (random.nextFloat() * 2.0f - 1.0f) * 0.2f;
                sample = juce::jlimit (-1.05f, 1.05f, sample);

                const float x = scope.getX() + t * scope.getWidth();
                const float y = scope.getCentreY() - sample * scope.getHeight() * 0.42f;
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);

                if (osc2Level > 0.01f)
                {
                    const float ph2 = std::fmod (t * 2.0f * osc2Ratio + phase2, 1.0f);
                    const float s2 = juce::jlimit (-1.05f, 1.05f,
                        shapeSample (osc2Wave, ph2, pulseWidth, false) * osc2Level);
                    const float y2 = scope.getCentreY() - s2 * scope.getHeight() * 0.42f;
                    if (i == 0) path2.startNewSubPath (x, y2); else path2.lineTo (x, y2);
                }
            }
            if (osc2Level > 0.01f)
            {
                g.setColour (accent.withAlpha (0.5f).interpolatedWith (juce::Colours::white, 0.25f));
                g.strokePath (path2, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
            }
            g.setColour (accent);
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            const float lfoDepth = raw ("LFO_DEPTH", 0.0f);
            auto dotArea = juce::Rectangle<float> (scope.getRight() - 30.0f, scope.getY() - 8.0f, 26.0f, 26.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawEllipse (dotArea, 1.0f);
            const float ang = lfoPhase * juce::MathConstants<float>::twoPi;
            const auto c = dotArea.getCentre();
            const float r = dotArea.getWidth() * 0.5f - 3.0f;
            const juce::Point<float> dot (c.x + std::cos (ang) * r, c.y + std::sin (ang) * r);
            g.setColour (accent.withAlpha (juce::jlimit (0.30f, 1.0f, 0.30f + lfoDepth * 0.9f)));
            g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre (dot));

            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.drawText ("DCO SCOPE", getLocalBounds().reduced (10, 5), juce::Justification::topLeft);
            g.drawText ("LFO", juce::Rectangle<float> (dotArea.getX() - 4.0f, dotArea.getBottom() + 1.0f, 34.0f, 10.0f).toNearestInt(),
                        juce::Justification::centred);
        }

    private:
        float raw (const juce::String& id, float fallback) const
        {
            if (auto* v = apvts.getRawParameterValue (id)) return v->load();
            return fallback;
        }

        // fiveWay selects the main DCO's 5-option numbering (2 = Saw+Pulse
        // blend, 3 = Triangle, 4 = Sine) vs. OSC2's 4-option numbering
        // (0 = Saw, 1 = Pulse, 2 = Triangle, 3 = Sine, no blend option).
        static float shapeSample (float waveIndexF, float ph, float pulseWidth, bool fiveWay) noexcept
        {
            const float saw = 2.0f * ph - 1.0f;
            const float pulse = ph < pulseWidth ? 1.0f : -1.0f;
            const float triangle = 4.0f * std::abs (ph - 0.5f) - 1.0f;
            const float sine = std::sin (juce::MathConstants<float>::twoPi * ph);

            if (fiveWay)
            {
                if (waveIndexF < 0.5f) return saw;
                if (waveIndexF < 1.5f) return pulse;
                if (waveIndexF < 2.5f) return 0.5f * (saw + pulse);
                if (waveIndexF < 3.5f) return triangle;
                if (waveIndexF < 4.5f) return sine;

                // Compact preview of the built-in modern wavetable. Keep the
                // visualizer consistent with the DSP without duplicating a
                // large table or adding any runtime allocation.
                const float harmonic = 0.62f * sine
                                      + 0.24f * std::sin (juce::MathConstants<float>::twoPi * 3.0f * ph)
                                      + 0.14f * std::sin (juce::MathConstants<float>::twoPi * 5.0f * ph);
                const float hollow = 0.72f * sine + 0.28f * std::sin (juce::MathConstants<float>::twoPi * 2.0f * ph);
                return 0.5f * harmonic + 0.5f * hollow;
            }
            if (waveIndexF < 0.5f) return saw;
            if (waveIndexF < 1.5f) return pulse;
            if (waveIndexF < 2.5f) return triangle;
            return sine;
        }

        void timerCallback() override
        {
            phase = std::fmod (phase + 0.012f, 1.0f);
            phase2 = std::fmod (phase2 + 0.012f, 1.0f);
            const float lfoRate = raw ("LFO_RATE", 1.0f);
            lfoPhase = std::fmod (lfoPhase + lfoRate * 0.0095f, 1.0f);
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent;
        float phase = 0.0f, phase2 = 0.0f, lfoPhase = 0.0f;
        juce::Random random;
    };

    //--------------------------------------------------------------------
    // Stylised ADSR shape for the amp envelope (ATTACK/DECAY/SUSTAIN/RELEASE
    // by default, but reusable for the filter envelope too).
    //--------------------------------------------------------------------
    class EnvelopeCurveView final : public juce::Component, private juce::Timer
    {
    public:
        EnvelopeCurveView (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour,
                            juce::String attackID, juce::String decayID,
                            juce::String sustainID, juce::String releaseID, juce::String captionText)
            : apvts (state), accent (accentColour),
              atkID (std::move (attackID)), decID (std::move (decayID)),
              susID (std::move (sustainID)), relID (std::move (releaseID)), caption (std::move (captionText))
        {
            startTimerHz (15);
        }

        ~EnvelopeCurveView() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colour::fromRGB (9, 8, 7));
            g.fillRoundedRectangle (b, 7.0f);
            g.setColour (accent.withAlpha (0.28f));
            g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.0f);

            auto area = b.reduced (10.0f, 16.0f);
            const float atk = raw (atkID, 0.01f);
            const float dec = raw (decID, 0.2f);
            const float sus = juce::jlimit (0.0f, 1.0f, raw (susID, 0.7f));
            const float rel = raw (relID, 0.3f);

            const float total = juce::jmax (0.05f, atk + dec + rel * 0.6f);
            const float wA = juce::jlimit (0.05f, 0.5f, atk / total);
            const float wD = juce::jlimit (0.05f, 0.4f, dec / total);
            const float wR = juce::jlimit (0.06f, 0.5f, (rel * 0.6f) / total);
            const float wS = juce::jmax (0.08f, 1.0f - wA - wD - wR);

            const float x0 = area.getX();
            const float x1 = x0 + area.getWidth() * wA;
            const float x2 = x1 + area.getWidth() * wD;
            const float x3 = x2 + area.getWidth() * wS;
            const float x4 = area.getRight();
            const float yTop = area.getY();
            const float yBase = area.getBottom();
            const float ySus = yBase - sus * area.getHeight();

            juce::Path path;
            path.startNewSubPath (x0, yBase);
            path.lineTo (x1, yTop);
            path.lineTo (x2, ySus);
            path.lineTo (x3, ySus);
            path.lineTo (x4, yBase);

            juce::Path fill (path);
            fill.lineTo (x0, yBase);
            fill.closeSubPath();
            g.setColour (accent.withAlpha (0.14f));
            g.fillPath (fill);

            g.setColour (accent);
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.drawText (caption, getLocalBounds().reduced (10, 5), juce::Justification::topLeft);
        }

    private:
        float raw (const juce::String& id, float fallback) const
        {
            if (auto* v = apvts.getRawParameterValue (id)) return v->load();
            return fallback;
        }
        void timerCallback() override { repaint(); }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent;
        juce::String atkID, decID, susID, relID, caption;
    };

    //--------------------------------------------------------------------
    // Stylised VCF frequency-response curve, reactive to CUTOFF/RESONANCE.
    //--------------------------------------------------------------------
    class FilterCurveView final : public juce::Component, private juce::Timer
    {
    public:
        FilterCurveView (juce::AudioProcessorValueTreeState& state, juce::Colour accentColour)
            : apvts (state), accent (accentColour)
        {
            startTimerHz (15);
        }

        ~FilterCurveView() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colour::fromRGB (9, 8, 7));
            g.fillRoundedRectangle (b, 7.0f);
            g.setColour (accent.withAlpha (0.28f));
            g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.0f);

            auto area = b.reduced (10.0f, 16.0f);
            const float cutoff = raw ("CUTOFF", 4200.0f);
            const float reso = juce::jlimit (0.0f, 1.0f, raw ("RESONANCE", 0.18f));
            const int filterType = (int) raw ("FILTER_TYPE", 0.0f);

            const float minF = std::log10 (40.0f), maxF = std::log10 (20000.0f);
            const float cutNorm = juce::jlimit (0.0f, 1.0f,
                (std::log10 (juce::jmax (40.0f, cutoff)) - minF) / (maxF - minF));

            // Shape depends on the selected Serum-style filter type: LP/HP
            // are a shelf-and-slope either side of cutoff (24 dB steeper
            // than 12 dB), BP is a bump centred on cutoff, Notch is a dip.
            juce::Path path;
            constexpr int n = 90;
            for (int i = 0; i <= n; ++i)
            {
                const float t = (float) i / (float) n;
                const float dist = t - cutNorm; // positive = above cutoff
                float mag = 1.0f;
                switch (filterType)
                {
                    case 0: // Juno LP 24dB
                    case 1: // LP 12dB
                    {
                        const float slope = filterType == 0 ? 0.08f : 0.14f;
                        if (dist <= 0.0f)
                        {
                            mag = 1.0f + (-dist < 0.08f ? reso * (1.0f - (-dist) / 0.08f) * 0.9f : 0.0f);
                        }
                        else
                        {
                            mag = std::pow (0.5f, dist / slope);
                            if (dist < 0.05f)
                                mag += reso * (1.0f - dist / 0.05f) * 0.9f;
                        }
                        break;
                    }
                    case 2: // HP 12dB
                    {
                        const float slope = 0.14f;
                        if (dist >= 0.0f)
                        {
                            mag = 1.0f + (dist < 0.08f ? reso * (1.0f - dist / 0.08f) * 0.9f : 0.0f);
                        }
                        else
                        {
                            mag = std::pow (0.5f, (-dist) / slope);
                            if (-dist < 0.05f)
                                mag += reso * (1.0f - (-dist) / 0.05f) * 0.9f;
                        }
                        break;
                    }
                    case 3: // BP 12dB
                    {
                        const float width = 0.09f + (1.0f - reso) * 0.14f;
                        mag = std::pow (0.5f, std::abs (dist) / width) * (0.55f + reso * 0.8f);
                        break;
                    }
                    default: // Notch 12dB
                    {
                        const float width = 0.05f + (1.0f - reso) * 0.05f;
                        mag = 1.0f - std::pow (0.5f, std::abs (dist) / width) * (0.85f + reso * 0.1f);
                        break;
                    }
                }
                const float x = area.getX() + t * area.getWidth();
                const float y = area.getBottom() - juce::jlimit (0.0f, 1.35f, mag) * area.getHeight() * 0.72f;
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            g.setColour (accent);
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            const float cx = area.getX() + cutNorm * area.getWidth();
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawVerticalLine ((int) cx, area.getY(), area.getBottom());

            static constexpr const char* typeNames[] = { "JUNO LP24", "LP12", "HP12", "BP12", "NOTCH" };
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.drawText (juce::String ("VCF RESPONSE - ") + typeNames[juce::jlimit (0, 4, filterType)],
                        getLocalBounds().reduced (10, 5), juce::Justification::topLeft);
        }

    private:
        float raw (const juce::String& id, float fallback) const
        {
            if (auto* v = apvts.getRawParameterValue (id)) return v->load();
            return fallback;
        }
        void timerCallback() override { repaint(); }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent;
    };

    struct JunoPreset
    {
        juce::String name;
        juce::String description;
        std::vector<std::pair<juce::String, float>> values;
    };

    // Every non-curve parameter's default value, mirroring
    // createParameterLayout()'s declared defaults exactly. Presets below
    // only list the parameters that matter for their own character; this
    // baseline is applied first so switching presets (or loading one after
    // hand-tweaking) always lands on exactly the sound the preset was
    // designed with, with nothing left over from before -- e.g. LEAD
    // SCREAM's unison detune no longer bleeds into the next preset you pick.
    inline const std::vector<std::pair<juce::String, float>>& junoPresetDefaults()
    {
        static const std::vector<std::pair<juce::String, float>> defaults = {
            { "WAVE", 2.0f }, { "PULSE", 0.50f }, { "PWM_RATE", 0.55f }, { "PWM_DEPTH", 0.0f },
            { "SUB", 0.35f }, { "SUB_OCT", 0.0f }, { "NOISE", 0.04f },
            { "WT_POS", 0.0f }, { "WT_LEVEL", 0.0f }, { "FM_AMOUNT", 0.0f }, { "HYBRID", 0.0f },
            { "HPF", 0.18f }, { "CUTOFF", 4200.0f }, { "RESONANCE", 0.18f }, { "FILTER_TYPE", 0.0f },
            { "ATTACK", 0.008f }, { "DECAY", 0.22f }, { "SUSTAIN", 0.72f }, { "RELEASE", 0.35f },
            { "LFO_RATE", 4.8f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.0f },
            { "TEMPOSYNC", 0.0f }, { "NOTEDIV", 4.0f },
            { "LFO2_RATE", 1.2f }, { "LFO2_DEPTH", 0.0f }, { "LFO2_PITCH", 0.0f },
            { "LFO1_DEST_A", 1.0f }, { "LFO1_AMT_A", 0.0f },
            { "LFO1_DEST_B", 0.0f }, { "LFO1_AMT_B", 0.0f }, { "LFO1_SMOOTH", 0.18f },
            { "MODENV_ATTACK", 0.02f }, { "MODENV_DECAY", 0.35f }, { "MODENV_AMOUNT", 0.0f },
            { "OSC2_WAVE", 0.0f }, { "OSC2_SEMI", 0.0f }, { "OSC2_FINE", 0.0f },
            { "OSC2_LEVEL", 0.0f }, { "OSC2_WT_POS", 0.0f },
            { "UNISON", 0.0f }, { "DETUNE", 7.0f }, { "DRIFT", 0.08f },
            { "FILTER_DRIVE", 0.10f }, { "KEYTRACK", 0.55f }, { "VEL_FILTER", 0.25f },
            { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.38f },
            { "DELAY_TIME", 280.0f }, { "DELAY_FEEDBACK", 0.18f }, { "DELAY_MIX", 0.0f },
            { "REVERB_MIX", 0.0f }, { "WIDTH", 0.72f }, { "DRIVE", 0.0f },
            { "GLIDE", 0.015f }, { "VEL_VCA", 0.0f }, { "LEVEL", -3.0f }
        };
        return defaults;
    }

    inline std::vector<JunoPreset> makeJunoPresets()
    {
        return {
            { "INIT", "Neutral starting point -- classic DCO/VCF/VCA with light chorus.", {
                { "WAVE", 2.0f }, { "PULSE", 0.50f }, { "PWM_RATE", 0.55f }, { "PWM_DEPTH", 0.0f },
                { "SUB", 0.35f }, { "SUB_OCT", 0.0f }, { "NOISE", 0.04f }, { "HPF", 0.18f },
                { "CUTOFF", 4200.0f }, { "RESONANCE", 0.18f },
                { "ATTACK", 0.008f }, { "DECAY", 0.22f }, { "SUSTAIN", 0.72f }, { "RELEASE", 0.35f },
                { "LFO_RATE", 4.8f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.38f }, { "WIDTH", 0.72f }, { "LEVEL", -3.0f } } },
            { "CLASSIC PAD", "Slow-attack Juno-106 pad -- PWM movement and lush chorus II.", {
                { "WAVE", 2.0f }, { "PULSE", 0.50f }, { "PWM_RATE", 0.30f }, { "PWM_DEPTH", 0.35f },
                { "SUB", 0.18f }, { "NOISE", 0.0f }, { "HPF", 0.10f },
                { "CUTOFF", 2400.0f }, { "RESONANCE", 0.14f },
                { "ATTACK", 0.60f }, { "DECAY", 0.80f }, { "SUSTAIN", 0.82f }, { "RELEASE", 1.30f },
                { "LFO_RATE", 3.2f }, { "LFO_DEPTH", 0.15f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 2.0f }, { "CHORUS_MIX", 0.62f }, { "WIDTH", 0.95f }, { "LEVEL", -5.0f } } },
            { "SUB BASS", "Tight, mono-friendly low end -- sub oscillator two octaves down.", {
                { "WAVE", 1.0f }, { "PULSE", 0.30f }, { "SUB", 0.80f }, { "SUB_OCT", 1.0f }, { "NOISE", 0.0f },
                { "HPF", 0.0f }, { "CUTOFF", 900.0f }, { "RESONANCE", 0.25f },
                { "ATTACK", 0.005f }, { "DECAY", 0.30f }, { "SUSTAIN", 0.60f }, { "RELEASE", 0.20f },
                { "LFO_RATE", 2.0f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 0.0f }, { "WIDTH", 0.30f }, { "LEVEL", -2.0f } } },
            { "STRING ENSEMBLE", "Wide, slow-swelling strings -- deep chorus II and full width.", {
                { "WAVE", 2.0f }, { "PULSE", 0.50f }, { "PWM_RATE", 0.30f }, { "PWM_DEPTH", 0.40f },
                { "SUB", 0.10f }, { "CUTOFF", 5200.0f }, { "RESONANCE", 0.10f },
                { "ATTACK", 0.30f }, { "DECAY", 1.0f }, { "SUSTAIN", 0.85f }, { "RELEASE", 1.5f },
                { "LFO_RATE", 4.5f }, { "LFO_DEPTH", 0.08f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 2.0f }, { "CHORUS_MIX", 0.7f }, { "WIDTH", 1.0f }, { "LEVEL", -5.0f } } },
            { "LEAD SCREAM", "Biting unison lead -- detuned OSC2 an octave down, driven filter.", {
                { "WAVE", 0.0f }, { "PULSE", 0.5f }, { "SUB", 0.0f },
                { "CUTOFF", 6000.0f }, { "RESONANCE", 0.45f },
                { "ATTACK", 0.005f }, { "DECAY", 0.15f }, { "SUSTAIN", 0.70f }, { "RELEASE", 0.25f },
                { "UNISON", 0.6f }, { "DETUNE", 9.0f }, { "DRIFT", 0.10f },
                { "OSC2_WAVE", 0.0f }, { "OSC2_SEMI", -12.0f }, { "OSC2_FINE", 4.0f }, { "OSC2_LEVEL", 0.35f },
                { "LFO_RATE", 5.5f }, { "LFO_DEPTH", 0.05f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.30f }, { "DRIVE", 0.30f }, { "LEVEL", -3.0f } } },
            { "GLASS BELLS", "Bright, decaying bell tones -- sine core with a fifth-up OSC2 layer.", {
                { "WAVE", 4.0f }, { "SUB", 0.0f }, { "NOISE", 0.0f }, { "HPF", 0.05f },
                { "CUTOFF", 9000.0f }, { "RESONANCE", 0.05f },
                { "ATTACK", 0.005f }, { "DECAY", 1.4f }, { "SUSTAIN", 0.0f }, { "RELEASE", 1.8f },
                { "OSC2_WAVE", 3.0f }, { "OSC2_SEMI", 7.0f }, { "OSC2_FINE", 3.0f }, { "OSC2_LEVEL", 0.45f },
                { "LFO_RATE", 5.0f }, { "LFO_DEPTH", 0.03f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 2.0f }, { "CHORUS_MIX", 0.55f }, { "WIDTH", 1.0f }, { "LEVEL", -4.0f } } },
            { "PLUCK", "Percussive plucked tone -- fast decay with a drawn filter-snap curve.", {
                // The percussive filter snap now comes from the drawn Mod
                // Shape curve (destination A = Cutoff) instead of a separate
                // filter-envelope amount: a fast amp DECAY to SUSTAIN 0 gives
                // the pluck its length, RESONANCE gives it bite, and routing
                // curve A to Cutoff at a fast, un-synced rate adds movement
                // on top -- open the FILTER / MOD SHAPE panel to redraw it.
                { "WAVE", 0.0f }, { "PULSE", 0.5f }, { "SUB", 0.15f }, { "NOISE", 0.0f }, { "HPF", 0.05f },
                { "CUTOFF", 1800.0f }, { "RESONANCE", 0.35f }, { "FILTER_TYPE", 1.0f },
                { "ATTACK", 0.002f }, { "DECAY", 0.25f }, { "SUSTAIN", 0.0f }, { "RELEASE", 0.18f },
                { "FILTER_DRIVE", 0.15f }, { "KEYTRACK", 0.65f },
                { "TEMPOSYNC", 0.0f }, { "LFO_RATE", 5.0f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.0f },
                { "LFO1_DEST_A", 1.0f }, { "LFO1_AMT_A", 0.55f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.25f }, { "WIDTH", 0.6f }, { "LEVEL", -4.0f } } },
            { "FILTER WOBBLE", "Rhythmic LFO-driven cutoff sweep -- held notes stay sustained.", {
                // LFO_FILTER driving the cutoff is the Serum-style "wobble" --
                // SUSTAIN stays high so held notes don't die, and the LFO
                // does all the movement.
                { "WAVE", 1.0f }, { "PULSE", 0.35f }, { "SUB", 0.40f }, { "SUB_OCT", 0.0f }, { "NOISE", 0.0f },
                { "HPF", 0.0f }, { "CUTOFF", 1200.0f }, { "RESONANCE", 0.50f }, { "FILTER_TYPE", 1.0f },
                { "ATTACK", 0.005f }, { "DECAY", 0.30f }, { "SUSTAIN", 0.80f }, { "RELEASE", 0.30f },
                { "FILTER_DRIVE", 0.20f }, { "KEYTRACK", 0.30f },
                { "LFO_RATE", 3.0f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.55f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.30f }, { "WIDTH", 0.7f }, { "LEVEL", -4.0f } } },
            { "WARM BRASS", "Rounded ensemble brass -- key-tracked filter, gentle OSC2 detune.", {
                { "WAVE", 2.0f }, { "PULSE", 0.42f }, { "SUB", 0.10f }, { "NOISE", 0.0f },
                { "CUTOFF", 3200.0f }, { "RESONANCE", 0.22f }, { "FILTER_TYPE", 0.0f },
                { "ATTACK", 0.06f }, { "DECAY", 0.35f }, { "SUSTAIN", 0.78f }, { "RELEASE", 0.30f },
                { "OSC2_WAVE", 0.0f }, { "OSC2_SEMI", 0.0f }, { "OSC2_FINE", 6.0f }, { "OSC2_LEVEL", 0.30f },
                { "KEYTRACK", 0.60f }, { "FILTER_DRIVE", 0.18f },
                { "LFO_RATE", 4.5f }, { "LFO_DEPTH", 0.04f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.35f }, { "WIDTH", 0.8f }, { "LEVEL", -4.0f } } },
            { "EP KEYS", "Electric-piano-style tine -- FM bite with a fast, short decay.", {
                { "WAVE", 4.0f }, { "FM_AMOUNT", 0.18f }, { "HYBRID", 0.25f }, { "SUB", 0.0f }, { "NOISE", 0.0f },
                { "CUTOFF", 5200.0f }, { "RESONANCE", 0.08f },
                { "ATTACK", 0.004f }, { "DECAY", 0.9f }, { "SUSTAIN", 0.15f }, { "RELEASE", 0.6f },
                { "OSC2_WAVE", 3.0f }, { "OSC2_SEMI", 12.0f }, { "OSC2_FINE", 2.0f }, { "OSC2_LEVEL", 0.25f },
                { "LFO_RATE", 4.8f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.0f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.30f }, { "WIDTH", 0.75f }, { "LEVEL", -4.0f } } },
            { "AMBIENT DRONE", "Slow-evolving wavetable drone -- long reverb and echo tail.", {
                { "WAVE", 5.0f }, { "HYBRID", 1.0f }, { "WT_POS", 5.0f }, { "WT_LEVEL", 0.6f }, { "SUB", 0.0f },
                { "CUTOFF", 1800.0f }, { "RESONANCE", 0.30f },
                { "ATTACK", 2.0f }, { "DECAY", 2.0f }, { "SUSTAIN", 0.9f }, { "RELEASE", 3.5f },
                { "LFO2_RATE", 0.15f }, { "LFO2_DEPTH", 0.5f }, { "LFO2_PITCH", 0.15f },
                { "REVERB_MIX", 0.55f }, { "DELAY_MIX", 0.25f }, { "DELAY_TIME", 520.0f }, { "DELAY_FEEDBACK", 0.35f },
                { "WIDTH", 1.0f }, { "CHORUS", 2.0f }, { "CHORUS_MIX", 0.6f }, { "LEVEL", -6.0f } } },
            { "TALKING WAH", "Vocal-like auto-wah -- high-pass sweep driven by the classic LFO.", {
                { "WAVE", 1.0f }, { "PULSE", 0.40f }, { "SUB", 0.20f }, { "NOISE", 0.0f }, { "HPF", 0.25f },
                { "CUTOFF", 1000.0f }, { "RESONANCE", 0.55f }, { "FILTER_TYPE", 2.0f },
                { "ATTACK", 0.01f }, { "DECAY", 0.25f }, { "SUSTAIN", 0.75f }, { "RELEASE", 0.30f },
                { "KEYTRACK", 0.20f }, { "FILTER_DRIVE", 0.25f },
                { "LFO_RATE", 2.2f }, { "LFO_DEPTH", 0.0f }, { "LFO_FILTER", 0.60f },
                { "CHORUS", 1.0f }, { "CHORUS_MIX", 0.20f }, { "WIDTH", 0.6f }, { "LEVEL", -4.0f } } },
            { "FAT UNISON PAD", "Wide, detuned modern pad -- 16-voice unison headroom, big chorus.", {
                { "WAVE", 0.0f }, { "SUB", 0.25f }, { "NOISE", 0.0f },
                { "UNISON", 0.85f }, { "DETUNE", 14.0f }, { "DRIFT", 0.15f },
                { "CUTOFF", 3600.0f }, { "RESONANCE", 0.15f },
                { "ATTACK", 0.45f }, { "DECAY", 0.60f }, { "SUSTAIN", 0.85f }, { "RELEASE", 1.6f },
                { "OSC2_WAVE", 0.0f }, { "OSC2_SEMI", -12.0f }, { "OSC2_LEVEL", 0.40f },
                { "CHORUS", 2.0f }, { "CHORUS_MIX", 0.65f }, { "WIDTH", 1.0f }, { "LEVEL", -6.0f } } },
        };
    }

    //====================================================================
    // Main editor content. This holds the entire hand-built UI at a single
    // fixed "native" resolution (baseWidth x baseHeight). It is wrapped by
    // JunoEmuEditor (below), which is the actual AudioProcessorEditor and
    // is responsible only for picking a UI size (100% / 75% / 50%) and
    // applying it as a transform on this component -- so every control,
    // knob and the drawable filter grid all scale together with correct
    // mouse-position mapping (JUCE remaps mouse events through a
    // Component's transform automatically), instead of needing a second,
    // separately-laid-out UI per size.
    //====================================================================
    class JunoEmuEditorContent final : public juce::Component, private juce::Timer
    {
    public:
        static constexpr int baseWidth = 1320;
        static constexpr int baseHeight = 760;

        std::function<void (float)> onSizeChanged;

        explicit JunoEmuEditorContent (juce::AudioProcessorValueTreeState& state)
            : apvts (state),
              sliderLnf (accent), modernLnf (accent),
              tooltipWindow (this, 600)
        {
            setOpaque (true);

            // Build every child component FIRST. setSize() below triggers an
            // immediate synchronous resized() call as part of applying the
            // new bounds -- if that fires before waveScope and the panels
            // exist, resized() dereferences null unique_ptrs. (Caught via
            // ASan: "member access within null pointer of type
            // WaveScopeDisplay" -- this was the FL Studio load crash.)
            buildHeader();
            buildVisualizers();
            buildClassicSections();
            buildModernSection();

            presets = makeJunoPresets();
            for (int i = 0; i < (int) presets.size(); ++i)
                presetBox.addItem (presets[(size_t) i].name, i + 2);
            presetBox.setSelectedId (1, juce::dontSendNotification);
            presetBox.onChange = [this] { applySelectedPreset(); };

            setSize (baseWidth, baseHeight);
            startTimerHz (15);
        }

        ~JunoEmuEditorContent() override
        {
            stopTimer();
            presetBox.setLookAndFeel (nullptr);
            sizeBox.setLookAndFeel (nullptr);
            for (auto& c : classicSliders) c.slider->setLookAndFeel (nullptr);
            for (auto& c : modernSliders) c.slider->setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            auto bgBase = juce::Colour::fromRGB (16, 13, 11);
            auto panelBase = juce::Colour::fromRGB (26, 22, 19);
            auto panelEdge = accent.withAlpha (0.30f);

            g.fillAll (bgBase);
            g.setColour (accent.withAlpha (0.03f));
            for (float x = 0.0f; x < (float) getWidth(); x += 26.0f)
                g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
            for (float y = 0.0f; y < (float) getHeight(); y += 26.0f)
                g.drawHorizontalLine ((int) y, 0.0f, (float) getWidth());

            auto outer = getLocalBounds().toFloat().reduced (12.0f);
            g.setColour (panelBase);
            g.fillRoundedRectangle (outer, 12.0f);
            g.setColour (panelEdge);
            g.drawRoundedRectangle (outer, 12.0f, 1.0f);

            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (footerText, footerArea, juce::Justification::centred);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (18, 14);

            auto header = bounds.removeFromTop (46);
            bounds.removeFromTop (8);
            auto visualRow = bounds.removeFromTop (104);
            bounds.removeFromTop (7);
            auto row1 = bounds.removeFromTop (166);
            bounds.removeFromTop (7);
            auto row2 = bounds.removeFromTop (132);
            bounds.removeFromTop (7);
            // The modern section is deliberately allowed to consume the
            // remaining height. The previous fixed 108 px row left a large
            // unused rectangle underneath the synth at the default size.
            auto row3 = bounds.removeFromTop (juce::jmax (120, bounds.getHeight() - 26));
            bounds.removeFromTop (5);
            footerArea = bounds.removeFromBottom (18);

            // Header: title | preset selector | size selector | brand
            title.setBounds (header.removeFromLeft (200));
            brand.setBounds (header.removeFromRight (juce::jmin (300, header.getWidth() / 2)));
            sizeBox.setBounds (header.removeFromRight (96).reduced (6, 6));
            header.removeFromRight (6);
            presetBox.setBounds (header.reduced (6, 6));

            // Visualizers: scope | envelope | filter response
            const int gap = 8;
            const int visW = (visualRow.getWidth() - gap * 2) / 3;
            waveScope->setBounds (visualRow.removeFromLeft (visW));
            visualRow.removeFromLeft (gap);
            envView->setBounds (visualRow.removeFromLeft (visW));
            visualRow.removeFromLeft (gap);
            filterView->setBounds (visualRow);

            // Row 1: DCO | OSC 2 | VCF, widths proportional to column count
            // (DCO 9 cols, OSC2 6 cols, VCF 7 cols -- HYBRID moved out of DCO
            // into its single home on the MODERN EXTRAS panel, see below)
            const float r1Cols = 9.0f + 6.0f + 7.0f;
            const int r1AvailW = row1.getWidth() - gap * 2;
            const int dcoW = (int) (r1AvailW * (9.0f / r1Cols));
            const int osc2W = (int) (r1AvailW * (6.0f / r1Cols));
            dcoPanel->setBounds (row1.removeFromLeft (dcoW));
            row1.removeFromLeft (gap);
            osc2Panel->setBounds (row1.removeFromLeft (osc2W));
            row1.removeFromLeft (gap);
            vcfPanel->setBounds (row1);

            // Row 2: LFO | ENV | MOD ENV | CHORUS, widths proportional to column count
            // (MOD ENV shrank to 3 cols now that it only holds the pitch mod
            // envelope -- the filter ADSR moved to the drawable Mod Shape)
            const float totalCols = 6.0f + 4.0f + 3.0f + 4.0f;
            const int availW = row2.getWidth() - gap * 3;
            const int lfoW = (int) (availW * (6.0f / totalCols));
            const int envW = (int) (availW * (4.0f / totalCols));
            const int fenvW = (int) (availW * (3.0f / totalCols));
            lfoPanel->setBounds (row2.removeFromLeft (lfoW));
            row2.removeFromLeft (gap);
            envPanel->setBounds (row2.removeFromLeft (envW));
            row2.removeFromLeft (gap);
            fenvPanel->setBounds (row2.removeFromLeft (fenvW));
            row2.removeFromLeft (gap);
            fxPanel->setBounds (row2);

            // Row 3: the modern controls and the new free-form modulation
            // editor share the remaining space. Nothing is left as a blank
            // decorative rectangle.
            const int row3Gap = 8;
            const int modW = juce::jlimit (620, 820, (int) (row3.getWidth() * 0.64f));
            modulationPanel->setBounds (row3.removeFromLeft (modW));
            row3.removeFromLeft (row3Gap);
            modernPanel->setBounds (row3);
        }

    private:
        struct SliderCtrl
        {
            std::unique_ptr<juce::Label> label;
            std::unique_ptr<juce::Slider> slider;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        };

        void timerCallback() override
        {
            for (auto& s : selectors)
                s->refreshFromParameter();
            for (auto& s : waveTabs)
                s->refreshFromParameter();
        }

        void buildHeader()
        {
            title.setText ("JUNO EMU", juce::dontSendNotification);
            title.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold)));
            title.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (title);

            brand.setText ("DEVKOMODO  -  ANALOG / MODERN HYBRID", juce::dontSendNotification);
            brand.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            brand.setColour (juce::Label::textColourId, juce::Colours::white.interpolatedWith (accent, 0.55f));
            brand.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (brand);

            presetBox.addItem ("MANUAL", 1);
            presetBox.setLookAndFeel (&modernLnf);
            presetBox.setTooltip ("Load a factory preset (still fully editable afterwards) -- "
                                   "its description appears at the bottom of the panel");
            addAndMakeVisible (presetBox);

            // UI size presets. The whole editor is built at one native
            // resolution and scaled as a unit (see JunoEmuEditor), so this
            // never has to re-run the layout -- it just picks how big that
            // fixed layout is drawn.
            sizeBox.addItem ("SIZE 100%", 1);
            sizeBox.addItem ("SIZE 75%", 2);
            sizeBox.addItem ("SIZE 50%", 3);
            sizeBox.setSelectedId (1, juce::dontSendNotification);
            sizeBox.setLookAndFeel (&modernLnf);
            sizeBox.setTooltip ("Scale the whole plugin window down for small screens");
            sizeBox.onChange = [this]
            {
                const float scale = sizeBox.getSelectedId() == 2 ? 0.75f
                                   : sizeBox.getSelectedId() == 3 ? 0.50f
                                                                   : 1.0f;
                if (onSizeChanged)
                    onSizeChanged (scale);
            };
            addAndMakeVisible (sizeBox);
        }

        void buildVisualizers()
        {
            waveScope = std::make_unique<WaveScopeDisplay> (apvts, accent);
            addAndMakeVisible (*waveScope);
            envView = std::make_unique<EnvelopeCurveView> (apvts, accent, "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VCA ENVELOPE");
            addAndMakeVisible (*envView);
            filterView = std::make_unique<FilterCurveView> (apvts, accent);
            addAndMakeVisible (*filterView);
        }

        void buildClassicSections()
        {
            dcoPanel = std::make_unique<PanelSection> ("DCO", accent);
            addAndMakeVisible (*dcoPanel);
            addWaveTabs (*dcoPanel, "WAVE", { "SAW", "PULSE", "SAW+PLS", "TRI", "SINE", "WT" },
                         "Sets the main oscillator's waveform. WT crossfades in the Wavetable "
                         "Shape/Level below instead of replacing the classic DCO outright.");
            addClassicSlider (*dcoPanel, "PULSE", "PW");
            addClassicSlider (*dcoPanel, "PWM_RATE", "PWM RT");
            addClassicSlider (*dcoPanel, "PWM_DEPTH", "PWM DEP");
            addClassicSlider (*dcoPanel, "SUB", "SUB");
            addSelector (*dcoPanel, "SUB_OCT", { "-1 OCT", "-2 OCT" }, "OCT",
                         "Sets how many octaves below the main pitch the sub oscillator plays.");
            addClassicSlider (*dcoPanel, "NOISE", "NOISE");
            addDropdown (*dcoPanel, "WT_POS",
                         { "SINE", "TRIANGLE", "SAW", "SQUARE", "SINE 2H", "ORGAN", "FORMANT", "BUZZ SAW" }, "SHAPE");
                        addClassicSlider (*dcoPanel, "WT_LEVEL", "WT LVL");

            osc2Panel = std::make_unique<PanelSection> ("OSC 2", accent);
            addAndMakeVisible (*osc2Panel);
            addWaveTabs (*osc2Panel, "OSC2_WAVE", { "SAW", "PULSE", "TRI", "SINE", "WT" },
                         "Sets the second oscillator's waveform.");
            addClassicSlider (*osc2Panel, "OSC2_SEMI", "SEMI");
            addClassicSlider (*osc2Panel, "OSC2_FINE", "FINE");
            addClassicSlider (*osc2Panel, "OSC2_LEVEL", "LEVEL");
            addDropdown (*osc2Panel, "OSC2_WT_POS",
                         { "SINE", "TRIANGLE", "SAW", "SQUARE", "SINE 2H", "ORGAN", "FORMANT", "BUZZ SAW" }, "SHAPE");
            addClassicSlider (*osc2Panel, "FM_AMOUNT", "FM");

            vcfPanel = std::make_unique<PanelSection> ("HPF / VCF", accent);
            addAndMakeVisible (*vcfPanel);
            addClassicSlider (*vcfPanel, "HPF", "HPF");
            addClassicSlider (*vcfPanel, "CUTOFF", "CUTOFF");
            addSelector (*vcfPanel, "FILTER_TYPE", { "JUNO LP24", "LP12", "HP12", "BP12", "NOTCH" }, "TYPE",
                         "Chooses the VCF's response shape. Juno LP24 is the classic 4-pole "
                         "low-pass; the rest are 12 dB/oct modes for creative filtering.");
            addClassicSlider (*vcfPanel, "RESONANCE", "RESO");
            addClassicSlider (*vcfPanel, "KEYTRACK", "KEY TRK");
            addClassicSlider (*vcfPanel, "VEL_FILTER", "VEL FLT");
            addClassicSlider (*vcfPanel, "FILTER_DRIVE", "DRIVE");

            lfoPanel = std::make_unique<PanelSection> ("LFO", accent);
            addAndMakeVisible (*lfoPanel);
            addClassicSlider (*lfoPanel, "LFO_DEPTH", "VIBRATO");
            addClassicSlider (*lfoPanel, "LFO_FILTER", "VCF LFO");
            addClassicSlider (*lfoPanel, "LFO2_RATE", "LFO2 RT");
            addClassicSlider (*lfoPanel, "LFO2_DEPTH", "LFO2 DEP");
            addClassicSlider (*lfoPanel, "LFO2_PITCH", "LFO2 PCH");

            envPanel = std::make_unique<PanelSection> ("ENVELOPE (VCA)", accent);
            addAndMakeVisible (*envPanel);
            addClassicSlider (*envPanel, "ATTACK", "ATK");
            addClassicSlider (*envPanel, "DECAY", "DEC");
            addClassicSlider (*envPanel, "SUSTAIN", "SUS");
            addClassicSlider (*envPanel, "RELEASE", "REL");

            fenvPanel = std::make_unique<PanelSection> ("MOD ENV (PITCH)", accent);
            addAndMakeVisible (*fenvPanel);
            addClassicSlider (*fenvPanel, "MODENV_ATTACK", "M.ATK");
            addClassicSlider (*fenvPanel, "MODENV_DECAY", "M.DEC");
            addClassicSlider (*fenvPanel, "MODENV_AMOUNT", "M.AMT");

            fxPanel = std::make_unique<PanelSection> ("CHORUS", accent);
            addAndMakeVisible (*fxPanel);
            addSelector (*fxPanel, "CHORUS", { "OFF", "I", "II" }, "MODE",
                         "Selects the classic Juno chorus mode, or turns it off. "
                         "I is subtle movement, II is wider and more pronounced.");
            addClassicSlider (*fxPanel, "CHORUS_MIX", "MIX");
            addClassicSlider (*fxPanel, "GLIDE", "GLIDE");
            addClassicSlider (*fxPanel, "VEL_VCA", "VEL VCA");
        }

        void buildModernSection()
        {
            modernPanel = std::make_unique<PanelSection> ("MODERN EXTRAS", accent);
            addAndMakeVisible (*modernPanel);
            addModernKnob (*modernPanel, "HYBRID", "A/D MIX");
            addModernKnob (*modernPanel, "UNISON", "UNISON");
            addModernKnob (*modernPanel, "DETUNE", "DETUNE");
            addModernKnob (*modernPanel, "DRIFT", "DRIFT");
            addModernKnob (*modernPanel, "DELAY_TIME", "DLY TIME");
            addModernKnob (*modernPanel, "DELAY_FEEDBACK", "DLY FDBK");
            addModernKnob (*modernPanel, "DELAY_MIX", "DLY MIX");
            addModernKnob (*modernPanel, "REVERB_MIX", "REVERB");
            addModernKnob (*modernPanel, "WIDTH", "WIDTH");
            addModernKnob (*modernPanel, "DRIVE", "DRIVE");
            addModernKnob (*modernPanel, "LEVEL", "LEVEL");

            modulationPanel = std::make_unique<DrawableLfoPanel> (apvts, accent);
            addAndMakeVisible (*modulationPanel);
        }

        void addClassicSlider (PanelSection& section, const juce::String& id, const juce::String& shortLabel)
        {
            SliderCtrl c;
            c.label = std::make_unique<juce::Label>();
            c.label->setText (shortLabel, juce::dontSendNotification);
            c.label->setJustificationType (juce::Justification::centred);
            c.label->setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
            c.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.68f));

            c.slider = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
            c.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 14);
            c.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
            c.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            c.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c.slider->setScrollWheelEnabled (false);
            c.slider->setLookAndFeel (&sliderLnf);
            if (auto* p = apvts.getParameter (id))
                c.slider->setTooltip (devkomodo::parameterTooltip (id, p->name));
            c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *c.slider);

            section.addAndMakeVisible (*c.label);
            section.addAndMakeVisible (*c.slider);
            section.addItem (c.label.get(), c.slider.get());
            classicSliders.push_back (std::move (c));
        }

        void addModernKnob (PanelSection& section, const juce::String& id, const juce::String& shortLabel)
        {
            SliderCtrl c;
            c.label = std::make_unique<juce::Label>();
            c.label->setText (shortLabel, juce::dontSendNotification);
            c.label->setJustificationType (juce::Justification::centred);
            c.label->setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
            c.label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.68f));

            c.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
            c.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
            c.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            c.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c.slider->setScrollWheelEnabled (false);
            c.slider->setLookAndFeel (&modernLnf);
            if (auto* p = apvts.getParameter (id))
                c.slider->setTooltip (devkomodo::parameterTooltip (id, p->name));
            c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *c.slider);

            section.addAndMakeVisible (*c.label);
            section.addAndMakeVisible (*c.slider);
            section.addItem (c.label.get(), c.slider.get());
            modernSliders.push_back (std::move (c));
        }

        void addWaveTabs (PanelSection& section, const juce::String& id, juce::StringArray labels,
                          const juce::String& tooltipText = {})
        {
            auto selector = std::make_unique<WaveTabSelector> (apvts, id, std::move (labels), accent, tooltipText);
            section.addAndMakeVisible (*selector);
            section.addItem (nullptr, selector.get());
            waveTabs.push_back (std::move (selector));
        }

        void addSelector (PanelSection& section, const juce::String& id, juce::StringArray labels,
                          const juce::String& captionText, const juce::String& tooltipText = {})
        {
            auto label = std::make_unique<juce::Label>();
            label->setText (captionText, juce::dontSendNotification);
            label->setJustificationType (juce::Justification::centred);
            label->setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
            label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.68f));
            section.addAndMakeVisible (*label);

            auto selector = std::make_unique<ChoiceSelector> (apvts, id, std::move (labels), accent, tooltipText);
            section.addAndMakeVisible (*selector);
            section.addItem (label.get(), selector.get());

            selectorLabels.push_back (std::move (label));
            selectors.push_back (std::move (selector));
        }

        // A genuine drop-down menu (juce::ComboBox), for choices where a
        // pill-button stack would take up too much vertical room or where
        // the person just wants to click a field and pick from a list --
        // e.g. the oscillator's wavetable shape.
        void addDropdown (PanelSection& section, const juce::String& id, juce::StringArray labels, const juce::String& captionText)
        {
            auto label = std::make_unique<juce::Label>();
            label->setText (captionText, juce::dontSendNotification);
            label->setJustificationType (juce::Justification::centred);
            label->setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
            label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.68f));
            section.addAndMakeVisible (*label);

            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList (labels, 1);
            box->setColour (juce::ComboBox::backgroundColourId, juce::Colour::fromRGB (30, 27, 24));
            box->setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.45f));
            box->setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.85f));
            box->setColour (juce::ComboBox::arrowColourId, accent);
            if (auto* p = apvts.getParameter (id))
                box->setTooltip (devkomodo::parameterTooltip (id, p->name));

            auto field = std::make_unique<DropdownField> (*box);
            section.addAndMakeVisible (*field);
            section.addItem (label.get(), field.get());

            dropdownAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *box));
            dropdownLabels.push_back (std::move (label));
            dropdownFields.push_back (std::move (field));
            dropdowns.push_back (std::move (box));
        }

        void applySelectedPreset()
        {
            const int id = presetBox.getSelectedId();
            if (id < 2 || id - 2 >= (int) presets.size())
            {
                footerText = defaultFooterText;
                repaint();
                return;
            }

            // Reset to the shared baseline first (see junoPresetDefaults()),
            // then layer the preset's own values on top, so every preset
            // load is fully deterministic regardless of what was selected
            // or hand-tweaked beforehand.
            for (const auto& [paramID, value] : junoPresetDefaults())
                if (auto* p = apvts.getParameter (paramID))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));

            const auto& preset = presets[(size_t) (id - 2)];
            for (const auto& [paramID, value] : preset.values)
                if (auto* p = apvts.getParameter (paramID))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));

            footerText = preset.description;
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent { juce::Colour::fromRGB (242, 148, 54) };

        JunoSliderLookAndFeel sliderLnf;
        DevKomodoKnobLookAndFeel modernLnf;
        juce::TooltipWindow tooltipWindow;

        juce::Label title, brand;
        juce::ComboBox presetBox;
        juce::ComboBox sizeBox;
        std::vector<JunoPreset> presets;
        const juce::String defaultFooterText { "16 VOICES  -  DCO / WT / FM / VCF / VCA  -  CHORUS I & II" };
        juce::String footerText { defaultFooterText };

        std::unique_ptr<WaveScopeDisplay> waveScope;
        std::unique_ptr<EnvelopeCurveView> envView;
        std::unique_ptr<FilterCurveView> filterView;

        std::unique_ptr<PanelSection> dcoPanel, osc2Panel, vcfPanel, lfoPanel, envPanel, fenvPanel, fxPanel, modernPanel;
        std::unique_ptr<DrawableLfoPanel> modulationPanel;

        std::vector<SliderCtrl> classicSliders, modernSliders;
        std::vector<std::unique_ptr<juce::Label>> selectorLabels;
        std::vector<std::unique_ptr<ChoiceSelector>> selectors;
        std::vector<std::unique_ptr<WaveTabSelector>> waveTabs;
        std::vector<std::unique_ptr<juce::Label>> dropdownLabels;
        std::vector<std::unique_ptr<DropdownField>> dropdownFields;
        std::vector<std::unique_ptr<juce::ComboBox>> dropdowns;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> dropdownAttachments;

        juce::Rectangle<int> footerArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JunoEmuEditorContent)
    };

    //====================================================================
    // Thin AudioProcessorEditor wrapper. It owns the fixed-resolution
    // JunoEmuEditorContent and, when the SIZE selector fires, scales that
    // content as a whole with an AffineTransform and resizes the actual
    // plugin window to match -- so 75%/50% are pixel-perfect shrinks of
    // the 100% layout rather than a second layout to maintain.
    //====================================================================
    class JunoEmuEditor final : public juce::AudioProcessorEditor
    {
    public:
        JunoEmuEditor (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& state)
            : AudioProcessorEditor (&processor)
        {
            setOpaque (true);
            content = std::make_unique<JunoEmuEditorContent> (state);
            content->onSizeChanged = [this] (float scale) { applyScale (scale); };
            addAndMakeVisible (*content);

            setResizable (false, false);
            setSize (JunoEmuEditorContent::baseWidth, JunoEmuEditorContent::baseHeight);
        }

        void paint (juce::Graphics&) override {}

        void resized() override
        {
            // content keeps its native size always; only its transform
            // (applied in applyScale) changes how big it looks on screen.
            content->setTopLeftPosition (0, 0);
        }

    private:
        void applyScale (float scale)
        {
            currentScale = scale;
            content->setTransform (juce::AffineTransform::scale (scale));
            setSize ((int) std::round ((float) JunoEmuEditorContent::baseWidth * scale),
                     (int) std::round ((float) JunoEmuEditorContent::baseHeight * scale));
        }

        std::unique_ptr<JunoEmuEditorContent> content;
        float currentScale = 1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JunoEmuEditor)
    };
}
