#pragma once

#include <cmath>
#include <algorithm>

// Lightweight PolyBLEP oscillator bank used by the audio-to-synth pedals.
// Waveforms: 0 sine, 1 saw, 2 square, 3 triangle, 4 pulse, 5 soft saw,
// 6 supersaw, 7 organ, 8 custom harmonic additive.
class BlepOscillator
{
public:
    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void setFrequency (float freqHz) noexcept
    {
        frequency = juce::jlimit (0.0f, (float) sampleRate * 0.49f, freqHz);
        phaseInc = frequency / (float) sampleRate;
    }

    void setWaveform (int type) noexcept { waveform = juce::jlimit (0, 8, type); }
    void setPulseWidth (float value) noexcept { pulseWidth = juce::jlimit (0.05f, 0.95f, value); }

    void setCustomHarmonics (float h2, float h3, float h4, float h5) noexcept
    {
        custom[0] = 1.0f; custom[1] = juce::jlimit (0.0f, 1.0f, h2);
        custom[2] = juce::jlimit (0.0f, 1.0f, h3); custom[3] = juce::jlimit (0.0f, 1.0f, h4);
        custom[4] = juce::jlimit (0.0f, 1.0f, h5);
    }

    void reset() noexcept { phase = 0.0f; triState = 0.0f; }

    float getNextSample() noexcept
    {
        float value = 0.0f;
        switch (waveform)
        {
            case 0: value = std::sin (juce::MathConstants<float>::twoPi * phase); break;
            case 1: value = renderSaw(); break;
            case 2: value = renderSquare(); break;
            case 3: value = renderTriangle(); break;
            case 4: value = renderPulse(); break;
            case 5: value = renderSoftSaw(); break;
            case 6: value = renderSuperSaw(); break;
            case 7: value = renderOrgan(); break;
            default: value = renderCustom(); break;
        }
        phase += phaseInc;
        if (phase >= 1.0f) phase -= std::floor (phase);
        return value;
    }

private:
    static float polyBlep (float t, float dt) noexcept
    {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float sawAt (float p, float detune = 1.0f) const noexcept
    {
        float v = 2.0f * p - 1.0f;
        v -= polyBlep (p, phaseInc * detune);
        return v;
    }

    float renderSaw() const noexcept { return sawAt (phase); }

    float renderSquare() const noexcept
    {
        float v = (phase < 0.5f) ? 1.0f : -1.0f;
        v += polyBlep (phase, phaseInc);
        float t2 = phase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
        v -= polyBlep (t2, phaseInc);
        return v;
    }

    float renderPulse() const noexcept
    {
        float v = (phase < pulseWidth) ? 1.0f : -1.0f;
        v += polyBlep (phase, phaseInc);
        float edge = phase - pulseWidth; if (edge < 0.0f) edge += 1.0f;
        v -= polyBlep (edge, phaseInc);
        return v;
    }

    float renderTriangle() noexcept
    {
        const float sq = renderSquare();
        triState += 4.0f * phaseInc * sq;
        triState *= 0.9995f;
        return juce::jlimit (-1.0f, 1.0f, triState);
    }

    float renderSoftSaw() const noexcept
    {
        // Rounded saw using a sine-shaped bend; intentionally gentler than a hard saw.
        const float x = 2.0f * phase - 1.0f;
        return x * (1.0f - 0.22f * (1.0f - x * x));
    }

    float renderSuperSaw() const noexcept
    {
        static constexpr float ratios[] = { 0.9970f, 0.9985f, 1.0f, 1.0015f, 1.0030f };
        float sum = 0.0f;
        for (float r : ratios)
        {
            float p = phase * r;
            p -= std::floor (p);
            sum += sawAt (p, r);
        }
        return sum * 0.4472136f;
    }

    float renderOrgan() const noexcept
    {
        const float twopi = juce::MathConstants<float>::twoPi;
        return 0.72f * std::sin (twopi * phase)
             + 0.42f * std::sin (twopi * phase * 2.0f)
             + 0.22f * std::sin (twopi * phase * 3.0f)
             + 0.10f * std::sin (twopi * phase * 4.0f);
    }

    float renderCustom() const noexcept
    {
        const float twopi = juce::MathConstants<float>::twoPi;
        float sum = 0.0f, norm = 0.0f;
        for (int h = 1; h <= 5; ++h)
        {
            const float amp = custom[h - 1];
            sum += amp * std::sin (twopi * phase * (float) h);
            norm += std::abs (amp);
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

    double sampleRate = 44100.0;
    float frequency = 220.0f;
    float phaseInc = 0.0f;
    float phase = 0.0f;
    float triState = 0.0f;
    float pulseWidth = 0.5f;
    float custom[5] = { 1.0f, 0.35f, 0.20f, 0.10f, 0.05f };
    int waveform = 0;
};
