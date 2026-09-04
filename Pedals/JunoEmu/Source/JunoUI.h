#pragma once

#include <JuceHeader.h>
#include "../../Common/ParameterTooltips.h"
#include "DevKomodoUI.h"
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <utility>

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
    // A stack of PillLedButtons bound directly to an AudioParameterChoice.
    //--------------------------------------------------------------------
    class ChoiceSelector final : public juce::Component
    {
    public:
        ChoiceSelector (juce::AudioProcessorValueTreeState& state, juce::String paramID,
                         juce::StringArray optionLabels, juce::Colour accentColour)
            : apvts (state), id (std::move (paramID)), labels (std::move (optionLabels))
        {
            for (int i = 0; i < labels.size(); ++i)
            {
                auto* btn = buttons.add (new PillLedButton (labels[i], accentColour));
                btn->onClick = [this, i] { select (i); };
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

            juce::Path path;
            constexpr int numPoints = 220;
            for (int i = 0; i <= numPoints; ++i)
            {
                const float t = (float) i / (float) numPoints;
                const float ph = std::fmod (t * 2.0f + phase, 1.0f);
                float sample;
                if (waveIndex < 0.5f)
                    sample = 2.0f * ph - 1.0f;
                else if (waveIndex < 1.5f)
                    sample = ph < pulseWidth ? 1.0f : -1.0f;
                else
                    sample = 0.5f * (2.0f * ph - 1.0f) + 0.5f * (ph < pulseWidth ? 1.0f : -1.0f);

                sample += subLevel * 0.4f * std::sin (juce::MathConstants<float>::twoPi * ph * 0.5f);
                sample += noiseLevel * (random.nextFloat() * 2.0f - 1.0f) * 0.2f;
                sample = juce::jlimit (-1.05f, 1.05f, sample);

                const float x = scope.getX() + t * scope.getWidth();
                const float y = scope.getCentreY() - sample * scope.getHeight() * 0.42f;
                if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
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

        void timerCallback() override
        {
            phase = std::fmod (phase + 0.012f, 1.0f);
            const float lfoRate = raw ("LFO_RATE", 1.0f);
            lfoPhase = std::fmod (lfoPhase + lfoRate * 0.0095f, 1.0f);
            repaint();
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent;
        float phase = 0.0f, lfoPhase = 0.0f;
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

            const float minF = std::log10 (40.0f), maxF = std::log10 (20000.0f);
            const float cutNorm = juce::jlimit (0.0f, 1.0f,
                (std::log10 (juce::jmax (40.0f, cutoff)) - minF) / (maxF - minF));

            juce::Path path;
            constexpr int n = 90;
            for (int i = 0; i <= n; ++i)
            {
                const float t = (float) i / (float) n;
                float mag;
                if (t < cutNorm)
                {
                    mag = 1.0f;
                    const float dist = cutNorm - t;
                    if (dist < 0.08f)
                        mag += reso * (1.0f - dist / 0.08f) * 0.9f;
                }
                else
                {
                    const float oct = (t - cutNorm) / 0.12f;
                    mag = std::pow (0.5f, oct);
                    const float distFromCut = t - cutNorm;
                    if (distFromCut < 0.05f)
                        mag += reso * (1.0f - distFromCut / 0.05f) * 0.9f;
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

            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
            g.drawText ("VCF RESPONSE", getLocalBounds().reduced (10, 5), juce::Justification::topLeft);
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
        std::vector<std::pair<juce::String, float>> values;
    };

    inline std::vector<JunoPreset> makeJunoPresets()
    {
        return {
            { "INIT", {
                { "WAVE", 2 }, { "PULSE", 0.50f }, { "PWM_RATE", 0.55f }, { "PWM_DEPTH", 0.0f },
                { "SUB", 0.35f }, { "SUB_OCT", 0 }, { "NOISE", 0.04f }, { "HPF", 0.18f },
                { "CUTOFF", 4200.0f }, { "RESONANCE", 0.18f }, { "ENV_AMOUNT", 0.45f },
                { "ATTACK", 0.008f }, { "DECAY", 0.22f }, { "SUSTAIN", 0.72f }, { "RELEASE", 0.35f },
                { "FILTER_ATTACK", 0.01f }, { "FILTER_DECAY", 0.25f },
                { "LFO_RATE", 4.8f }, { "LFO_DEPTH", 0.0f },
                { "CHORUS", 1 }, { "CHORUS_MIX", 0.38f }, { "WIDTH", 0.72f }, { "LEVEL", -3.0f } } },
            { "CLASSIC PAD", {
                { "WAVE", 2 }, { "PULSE", 0.50f }, { "PWM_RATE", 0.30f }, { "PWM_DEPTH", 0.35f },
                { "SUB", 0.18f }, { "NOISE", 0.0f }, { "HPF", 0.10f },
                { "CUTOFF", 2400.0f }, { "RESONANCE", 0.14f }, { "ENV_AMOUNT", 0.30f },
                { "ATTACK", 0.60f }, { "DECAY", 0.80f }, { "SUSTAIN", 0.82f }, { "RELEASE", 1.30f },
                { "FILTER_ATTACK", 0.45f }, { "FILTER_DECAY", 0.60f },
                { "LFO_RATE", 3.2f }, { "LFO_DEPTH", 0.15f },
                { "CHORUS", 2 }, { "CHORUS_MIX", 0.62f }, { "WIDTH", 0.95f }, { "LEVEL", -5.0f } } },
            { "SUB BASS", {
                { "WAVE", 1 }, { "PULSE", 0.30f }, { "SUB", 0.80f }, { "SUB_OCT", 1 }, { "NOISE", 0.0f },
                { "HPF", 0.0f }, { "CUTOFF", 900.0f }, { "RESONANCE", 0.25f }, { "ENV_AMOUNT", 0.5f },
                { "ATTACK", 0.005f }, { "DECAY", 0.30f }, { "SUSTAIN", 0.60f }, { "RELEASE", 0.20f },
                { "FILTER_ATTACK", 0.01f }, { "FILTER_DECAY", 0.20f },
                { "LFO_RATE", 2.0f }, { "LFO_DEPTH", 0.0f },
                { "CHORUS", 0 }, { "WIDTH", 0.30f }, { "LEVEL", -2.0f } } },
            { "STRING ENSEMBLE", {
                { "WAVE", 2 }, { "PULSE", 0.50f }, { "PWM_RATE", 0.30f }, { "PWM_DEPTH", 0.40f },
                { "SUB", 0.10f }, { "CUTOFF", 5200.0f }, { "RESONANCE", 0.10f }, { "ENV_AMOUNT", 0.20f },
                { "ATTACK", 0.30f }, { "DECAY", 1.0f }, { "SUSTAIN", 0.85f }, { "RELEASE", 1.5f },
                { "LFO_RATE", 4.5f }, { "LFO_DEPTH", 0.08f },
                { "CHORUS", 2 }, { "CHORUS_MIX", 0.7f }, { "WIDTH", 1.0f }, { "LEVEL", -5.0f } } },
            { "LEAD SCREAM", {
                { "WAVE", 0 }, { "PULSE", 0.5f }, { "SUB", 0.0f },
                { "CUTOFF", 6000.0f }, { "RESONANCE", 0.45f }, { "ENV_AMOUNT", 0.6f },
                { "ATTACK", 0.005f }, { "DECAY", 0.15f }, { "SUSTAIN", 0.70f }, { "RELEASE", 0.25f },
                { "UNISON", 0.6f }, { "DETUNE", 9.0f }, { "DRIFT", 0.10f },
                { "LFO_RATE", 5.5f }, { "LFO_DEPTH", 0.05f },
                { "CHORUS", 1 }, { "CHORUS_MIX", 0.30f }, { "DRIVE", 0.30f }, { "LEVEL", -3.0f } } },
        };
    }

    //====================================================================
    // Main editor.
    //====================================================================
    class JunoEmuEditor final : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        JunoEmuEditor (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& state)
            : AudioProcessorEditor (&processor), apvts (state),
              sliderLnf (accent), modernLnf (accent),
              tooltipWindow (this, 600)
        {
            setOpaque (true);
            setResizable (true, true);
            setResizeLimits (1000, 620, 1500, 940);

            buildHeader();
            buildVisualizers();
            buildClassicSections();
            buildModernSection();

            presets = makeJunoPresets();
            for (int i = 0; i < (int) presets.size(); ++i)
                presetBox.addItem (presets[(size_t) i].name, i + 2);
            presetBox.setSelectedId (1, juce::dontSendNotification);
            presetBox.onChange = [this] { applySelectedPreset(); };

            setSize (1180, 680);
            startTimerHz (15);
        }

        ~JunoEmuEditor() override
        {
            stopTimer();
            presetBox.setLookAndFeel (nullptr);
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
            g.drawText ("6 VOICES  \u2022  DCO / HPF / VCF / VCA  \u2022  CHORUS I & II",
                        footerArea, juce::Justification::centred);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (18, 14);

            auto header = bounds.removeFromTop (46);
            bounds.removeFromTop (8);
            auto visualRow = bounds.removeFromTop (110);
            bounds.removeFromTop (8);
            auto row1 = bounds.removeFromTop (150);
            bounds.removeFromTop (8);
            auto row2 = bounds.removeFromTop (120);
            bounds.removeFromTop (8);
            auto row3 = bounds.removeFromTop (108);
            bounds.removeFromTop (6);
            footerArea = bounds.removeFromTop (18);

            // Header: title | preset selector | brand
            title.setBounds (header.removeFromLeft (200));
            brand.setBounds (header.removeFromRight (juce::jmin (300, header.getWidth() / 2)));
            presetBox.setBounds (header.reduced (6, 6));

            // Visualizers: scope | envelope | filter response
            const int gap = 8;
            const int visW = (visualRow.getWidth() - gap * 2) / 3;
            waveScope->setBounds (visualRow.removeFromLeft (visW));
            visualRow.removeFromLeft (gap);
            envView->setBounds (visualRow.removeFromLeft (visW));
            visualRow.removeFromLeft (gap);
            filterView->setBounds (visualRow);

            // Row 1: DCO (wider) | VCF
            const int r1DcoW = (int) (row1.getWidth() * 0.52f);
            dcoPanel->setBounds (row1.removeFromLeft (r1DcoW));
            row1.removeFromLeft (gap);
            vcfPanel->setBounds (row1);

            // Row 2: LFO | ENV | FILTER ENV | CHORUS, widths proportional to column count
            const float totalCols = 2.0f + 4.0f + 2.0f + 2.0f;
            const int availW = row2.getWidth() - gap * 3;
            const int lfoW = (int) (availW * (2.0f / totalCols));
            const int envW = (int) (availW * (4.0f / totalCols));
            const int fenvW = (int) (availW * (2.0f / totalCols));
            lfoPanel->setBounds (row2.removeFromLeft (lfoW));
            row2.removeFromLeft (gap);
            envPanel->setBounds (row2.removeFromLeft (envW));
            row2.removeFromLeft (gap);
            fenvPanel->setBounds (row2.removeFromLeft (fenvW));
            row2.removeFromLeft (gap);
            fxPanel->setBounds (row2);

            // Row 3: modern extras, one wide panel
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
        }

        void buildHeader()
        {
            title.setText ("JUNO EMU", juce::dontSendNotification);
            title.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold)));
            title.setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (title);

            brand.setText ("DEVKOMODO  \u2022  DCO CLASSIC  \u2022  MODERN EDITION", juce::dontSendNotification);
            brand.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            brand.setColour (juce::Label::textColourId, juce::Colours::white.interpolatedWith (accent, 0.55f));
            brand.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (brand);

            presetBox.addItem ("MANUAL", 1);
            presetBox.setLookAndFeel (&modernLnf);
            presetBox.setTooltip ("Load a factory preset (still fully editable afterwards)");
            addAndMakeVisible (presetBox);
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
            addSelector (*dcoPanel, "WAVE", { "SAW", "PULSE", "SAW+PLS" }, "WAVE");
            addClassicSlider (*dcoPanel, "PULSE", "PW");
            addClassicSlider (*dcoPanel, "PWM_RATE", "PWM RT");
            addClassicSlider (*dcoPanel, "PWM_DEPTH", "PWM DEP");
            addClassicSlider (*dcoPanel, "SUB", "SUB");
            addSelector (*dcoPanel, "SUB_OCT", { "-1 OCT", "-2 OCT" }, "OCT");
            addClassicSlider (*dcoPanel, "NOISE", "NOISE");

            vcfPanel = std::make_unique<PanelSection> ("HPF / VCF", accent);
            addAndMakeVisible (*vcfPanel);
            addClassicSlider (*vcfPanel, "HPF", "HPF");
            addClassicSlider (*vcfPanel, "CUTOFF", "CUTOFF");
            addClassicSlider (*vcfPanel, "RESONANCE", "RESO");
            addClassicSlider (*vcfPanel, "ENV_AMOUNT", "ENV AMT");
            addClassicSlider (*vcfPanel, "KEYTRACK", "KEY TRK");
            addClassicSlider (*vcfPanel, "VEL_FILTER", "VEL FLT");
            addClassicSlider (*vcfPanel, "FILTER_DRIVE", "DRIVE");

            lfoPanel = std::make_unique<PanelSection> ("LFO", accent);
            addAndMakeVisible (*lfoPanel);
            addClassicSlider (*lfoPanel, "LFO_RATE", "RATE");
            addClassicSlider (*lfoPanel, "LFO_DEPTH", "VIBRATO");

            envPanel = std::make_unique<PanelSection> ("ENVELOPE (VCA)", accent);
            addAndMakeVisible (*envPanel);
            addClassicSlider (*envPanel, "ATTACK", "ATK");
            addClassicSlider (*envPanel, "DECAY", "DEC");
            addClassicSlider (*envPanel, "SUSTAIN", "SUS");
            addClassicSlider (*envPanel, "RELEASE", "REL");

            fenvPanel = std::make_unique<PanelSection> ("FILTER ENV", accent);
            addAndMakeVisible (*fenvPanel);
            addClassicSlider (*fenvPanel, "FILTER_ATTACK", "F.ATK");
            addClassicSlider (*fenvPanel, "FILTER_DECAY", "F.DEC");

            fxPanel = std::make_unique<PanelSection> ("CHORUS", accent);
            addAndMakeVisible (*fxPanel);
            addSelector (*fxPanel, "CHORUS", { "OFF", "I", "II" }, "MODE");
            addClassicSlider (*fxPanel, "CHORUS_MIX", "MIX");
        }

        void buildModernSection()
        {
            modernPanel = std::make_unique<PanelSection> ("MODERN EXTRAS", accent);
            addAndMakeVisible (*modernPanel);
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

        void addSelector (PanelSection& section, const juce::String& id, juce::StringArray labels, const juce::String& captionText)
        {
            auto label = std::make_unique<juce::Label>();
            label->setText (captionText, juce::dontSendNotification);
            label->setJustificationType (juce::Justification::centred);
            label->setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
            label->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.68f));
            section.addAndMakeVisible (*label);

            auto selector = std::make_unique<ChoiceSelector> (apvts, id, std::move (labels), accent);
            section.addAndMakeVisible (*selector);
            section.addItem (label.get(), selector.get());

            selectorLabels.push_back (std::move (label));
            selectors.push_back (std::move (selector));
        }

        void applySelectedPreset()
        {
            const int id = presetBox.getSelectedId();
            if (id < 2 || id - 2 >= (int) presets.size())
                return;
            for (const auto& [paramID, value] : presets[(size_t) (id - 2)].values)
                if (auto* p = apvts.getParameter (paramID))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Colour accent { juce::Colour::fromRGB (242, 148, 54) };

        JunoSliderLookAndFeel sliderLnf;
        DevKomodoKnobLookAndFeel modernLnf;
        juce::TooltipWindow tooltipWindow;

        juce::Label title, brand;
        juce::ComboBox presetBox;
        std::vector<JunoPreset> presets;

        std::unique_ptr<WaveScopeDisplay> waveScope;
        std::unique_ptr<EnvelopeCurveView> envView;
        std::unique_ptr<FilterCurveView> filterView;

        std::unique_ptr<PanelSection> dcoPanel, vcfPanel, lfoPanel, envPanel, fenvPanel, fxPanel, modernPanel;

        std::vector<SliderCtrl> classicSliders, modernSliders;
        std::vector<std::unique_ptr<juce::Label>> selectorLabels;
        std::vector<std::unique_ptr<ChoiceSelector>> selectors;

        juce::Rectangle<int> footerArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JunoEmuEditor)
    };
}
