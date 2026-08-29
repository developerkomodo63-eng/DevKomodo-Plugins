#pragma once

#include <JuceHeader.h>
#include <utility>
#include <vector>

namespace devkomodo
{
inline std::vector<std::pair<juce::String, float>> curatedPresetValues (const juce::String& category,
                                                                         int presetIndex)
{
    if (presetIndex <= 0)
        return {};

    const int index = juce::jlimit (0, 3, presetIndex - 1);
    std::vector<std::pair<juce::String, float>> values;
    const auto add = [&] (const char* id, float value) { values.emplace_back (id, value); };

    if (category.contains ("OVERDRIVE") || category.contains ("SATURAT") || category.contains ("CLIPPER"))
    {
        static constexpr float drive[] = { 1.8f, 3.8f, 5.6f, 7.5f };
        static constexpr float tone[] = { 3600.0f, 4400.0f, 5200.0f, 6100.0f };
        static constexpr float character[] = { 0.25f, 0.42f, 0.60f, 0.78f };
        static constexpr float bias[] = { -0.35f, 0.0f, 0.25f, 0.55f };
        add ("DRIVE", drive[index]); add ("TONE", tone[index]); add ("CHARACTER", character[index]); add ("BIAS", bias[index]);
    }
    else if (category.contains ("DISTORT"))
    {
        static constexpr float drive[] = { 2.2f, 4.8f, 6.8f, 8.8f };
        static constexpr float scoop[] = { 0.08f, 0.18f, 0.30f, 0.42f };
        static constexpr float tone[] = { 3200.0f, 3800.0f, 4600.0f, 5400.0f };
        static constexpr float bias[] = { -0.25f, 0.0f, 0.20f, 0.50f };
        add ("DRIVE", drive[index]); add ("SCOOP", scoop[index]); add ("TONE", tone[index]); add ("BIAS", bias[index]);
    }
    else if (category.contains ("FUZZ"))
    {
        static constexpr float fuzz[] = { 4.0f, 6.5f, 8.0f, 9.4f };
        static constexpr float drive[] = { 1.6f, 3.2f, 5.0f, 7.2f };
        static constexpr float tone[] = { 2600.0f, 3400.0f, 4300.0f, 5200.0f };
        static constexpr float bias[] = { -0.10f, 0.15f, 0.40f, 0.65f };
        add ("FUZZ", fuzz[index]); add ("DRIVE", drive[index]); add ("TONE", tone[index]); add ("BIAS", bias[index]);
    }
    else if (category.contains ("CONSOLE DRIVE"))
    {
        static constexpr float drive[] = { 1.0f, 2.8f, 4.5f, 6.0f };
        static constexpr float tone[] = { -1.0f, 0.0f, 1.5f, 2.5f };
        add ("DRIVE", drive[index]); add ("TONE", tone[index]);
    }
    else if (category.contains ("BOOST"))
    {
        static constexpr float gain[] = { 1.5f, 3.5f, 5.5f, 7.0f };
        static constexpr float bass[] = { -1.0f, 0.0f, 1.0f, 2.0f };
        add ("GAIN", gain[index]); add ("BASS", bass[index]);
    }
    else if (category.contains ("PREAMP"))
    {
        static constexpr float drive[] = { 0.8f, 2.0f, 3.8f, 5.5f };
        static constexpr float blend[] = { 0.55f, 0.70f, 0.82f, 0.92f };
        add ("DRIVE", drive[index]); add ("BLEND", blend[index]);
    }
    else if (category.contains ("NOISE GATE"))
    {
        static constexpr float threshold[] = { -58.0f, -50.0f, -43.0f, -35.0f };
        static constexpr float release[] = { 180.0f, 120.0f, 80.0f, 45.0f };
        add ("THRESHOLD", threshold[index]); add ("RELEASE", release[index]);
    }
    else if (category.contains ("CHORUS") || category.contains ("DOUBLER"))
    {
        static constexpr float rate[] = { 0.35f, 0.70f, 1.20f, 2.20f };
        static constexpr float depth[] = { 0.25f, 0.42f, 0.62f, 0.78f };
        static constexpr float mix[] = { 0.20f, 0.32f, 0.45f, 0.58f };
        static constexpr float spread[] = { 0.18f, 0.32f, 0.52f, 0.78f };
        add ("RATE", rate[index]); add ("DEPTH", depth[index]); add ("MIX", mix[index]); add ("SPREAD", spread[index]);
    }
    else if (category.contains ("FLANGER"))
    {
        static constexpr float rate[] = { 0.18f, 0.42f, 0.85f, 1.60f };
        static constexpr float depth[] = { 0.25f, 0.48f, 0.68f, 0.82f };
        static constexpr float feedback[] = { 0.15f, 0.35f, 0.58f, 0.72f };
        static constexpr float width[] = { 0.20f, 0.38f, 0.58f, 0.76f };
        add ("RATE", rate[index]); add ("DEPTH", depth[index]); add ("FEEDBACK", feedback[index]); add ("WIDTH", width[index]);
    }
    else if (category.contains ("PHASER"))
    {
        static constexpr float rate[] = { 0.20f, 0.45f, 0.90f, 1.80f };
        static constexpr float depth[] = { 0.30f, 0.50f, 0.70f, 0.88f };
        add ("RATE", rate[index]); add ("DEPTH", depth[index]);
    }
    else if (category.contains ("TREMOLO") || category.contains ("VIBRATO"))
    {
        static constexpr float rate[] = { 1.4f, 3.0f, 5.5f, 8.0f };
        static constexpr float depth[] = { 0.22f, 0.42f, 0.64f, 0.82f };
        static constexpr float bias[] = { 0.10f, 0.22f, 0.40f, 0.55f };
        add ("RATE", rate[index]); add ("DEPTH", depth[index]); add ("DRIFT", bias[index]);
    }
    else if (category.contains ("ROTARY SPEAKER"))
    {
        static constexpr float speed[] = { 0.5f, 1.2f, 2.4f, 4.0f };
        static constexpr float depth[] = { 0.35f, 0.55f, 0.72f, 0.88f };
        add ("SPEED", speed[index]); add ("DEPTH", depth[index]);
    }
    else if (category.contains ("DOUBLER"))
    {
        static constexpr float delay[] = { 8.0f, 14.0f, 22.0f, 30.0f };
        static constexpr float width[] = { 0.35f, 0.55f, 0.75f, 0.92f };
        add ("DELAY", delay[index]); add ("WIDTH", width[index]);
    }
    else if (category.contains ("DELAY") && ! category.contains ("REVERB"))
    {
        static constexpr float time[] = { 90.0f, 180.0f, 360.0f, 620.0f };
        static constexpr float feedback[] = { 0.18f, 0.32f, 0.48f, 0.64f };
        static constexpr float mix[] = { 0.16f, 0.24f, 0.32f, 0.40f };
        add ("TIME", time[index]); add ("FEEDBACK", feedback[index]); add ("MIX", mix[index]);
    }
    else if (category.contains ("SPRING REVERB"))
    {
        static constexpr float decay[] = { 0.28f, 0.48f, 0.68f, 0.86f };
        static constexpr float mix[] = { 0.18f, 0.26f, 0.35f, 0.44f };
        add ("DECAY", decay[index]); add ("MIX", mix[index]);
    }
    else if (category.contains ("CONVOLUTION REVERB"))
    {
        static constexpr float mix[] = { 0.16f, 0.24f, 0.34f, 0.46f };
        add ("MIX", mix[index]);
    }
    else if (category.contains ("UTILITY GAIN"))
    {
        static constexpr float gain[] = { -3.0f, 0.0f, 3.0f, 6.0f };
        add ("GAIN", gain[index]);
    }
    else if (category.contains ("MONO MAKER"))
    {
        static constexpr float frequency[] = { 80.0f, 140.0f, 220.0f, 360.0f };
        add ("FREQUENCY", frequency[index]);
    }
    else if (category.contains ("AIR ENHANCER"))
    {
        static constexpr float low[] = { 0.15f, 0.28f, 0.38f, 0.48f };
        static constexpr float high[] = { 0.25f, 0.42f, 0.62f, 0.78f };
        add ("LOW", low[index]); add ("HIGH", high[index]);
    }
    else if (category.contains ("EXCITER"))
    {
        static constexpr float frequency[] = { 2800.0f, 4200.0f, 6000.0f, 7800.0f };
        static constexpr float amount[] = { 0.18f, 0.32f, 0.48f, 0.66f };
        static constexpr float mix[] = { 0.24f, 0.34f, 0.44f, 0.52f };
        add ("FREQUENCY", frequency[index]); add ("AMOUNT", amount[index]); add ("MIX", mix[index]);
    }
    else if (category.contains ("AUTO SWELL"))
    {
        static constexpr float time[] = { 180.0f, 420.0f, 850.0f, 1450.0f };
        static constexpr float threshold[] = { -48.0f, -42.0f, -36.0f, -30.0f };
        add ("SWELLTIME", time[index]); add ("THRESHOLD", threshold[index]);
    }
    else if (category.contains ("DE-ESSER") || category.contains ("DEESSER"))
    {
        static constexpr float frequency[] = { 4500.0f, 5500.0f, 6500.0f, 7600.0f };
        static constexpr float threshold[] = { -24.0f, -20.0f, -16.0f, -12.0f };
        static constexpr float ratio[] = { 3.0f, 5.0f, 8.0f, 12.0f };
        add ("FREQUENCY", frequency[index]); add ("THRESHOLD", threshold[index]); add ("RATIO", ratio[index]);
    }
    else if (category.contains ("GLITCH"))
    {
        static constexpr float rate[] = { 80.0f, 150.0f, 260.0f, 420.0f };
        static constexpr float chaos[] = { 0.18f, 0.35f, 0.56f, 0.78f };
        static constexpr float depth[] = { 0.35f, 0.55f, 0.72f, 0.90f };
        add ("RATE", rate[index]); add ("CHAOS", chaos[index]); add ("DEPTH", depth[index]);
    }
    else if (category.contains ("TAPE EMULATION"))
    {
        static constexpr float wow[] = { 0.08f, 0.18f, 0.32f, 0.48f };
        static constexpr float flutter[] = { 0.06f, 0.14f, 0.25f, 0.38f };
        static constexpr float saturation[] = { 0.18f, 0.32f, 0.50f, 0.70f };
        static constexpr float hiss[] = { 0.02f, 0.06f, 0.12f, 0.20f };
        add ("WOW", wow[index]); add ("FLUTTER", flutter[index]);
        add ("SATURATION", saturation[index]); add ("HISS", hiss[index]);
    }
    else if (category.contains ("EQ") || category.contains ("TONE") || category.contains ("FILTER"))
    {
        static constexpr float bass[] = { -1.0f, 0.0f, 1.5f, 2.8f };
        static constexpr float mid[] = { -0.5f, 0.2f, 1.0f, 1.8f };
        static constexpr float treble[] = { 0.2f, 0.8f, 1.6f, 2.4f };
        static constexpr float cutoff[] = { 2400.0f, 4200.0f, 6400.0f, 9000.0f };
        static constexpr float q[] = { 0.20f, 0.33f, 0.48f, 0.66f };
        static constexpr float gain[] = { -3.0f, 0.0f, 2.0f, 5.0f };
        add ("BASS", bass[index]); add ("MID", mid[index]); add ("TREBLE", treble[index]);
        add ("CUTOFF", cutoff[index]); add ("Q", q[index]); add ("GAIN", gain[index]);
    }
    else if (category.contains ("COMPRESS") || category.contains ("LIMIT") || category.contains ("GATE"))
    {
        static constexpr float threshold[] = { -32.0f, -26.0f, -22.0f, -16.0f };
        static constexpr float ratio[] = { 1.5f, 2.5f, 4.0f, 6.0f };
        static constexpr float attack[] = { 12.0f, 20.0f, 30.0f, 50.0f };
        static constexpr float release[] = { 180.0f, 120.0f, 80.0f, 45.0f };
        static constexpr float mix[] = { 0.18f, 0.30f, 0.42f, 0.58f };
        static constexpr float gain[] = { -2.0f, 0.0f, 1.5f, 3.0f };
        add ("THRESHOLD", threshold[index]); add ("RATIO", ratio[index]);
        add ("ATTACK", attack[index]); add ("RELEASE", release[index]);
        add ("MIX", mix[index]); add ("GAIN", gain[index]);
    }
    else if (category.contains ("VOCAL SHIFTER"))
    {
        static constexpr float pitch[] = { 0.0f, 7.0f, -5.0f, 12.0f };
        static constexpr float formant[] = { 0.0f, 2.0f, -2.0f, 4.0f };
        static constexpr float mix[] = { 0.70f, 0.82f, 0.78f, 0.65f };
        add ("PITCH", pitch[index]); add ("FORMANT", formant[index]); add ("MIX", mix[index]);
    }
    else if (category.contains ("RING MODULATOR"))
    {
        static constexpr float frequency[] = { 30.0f, 90.0f, 220.0f, 440.0f };
        add ("FREQUENCY", frequency[index]);
    }
    else if (category.contains ("SYNTH") || category.contains ("WAVEFORM") || category.contains ("OSCILLATOR"))
    {
        static constexpr float waveform[] = { 0.0f, 1.0f, 6.0f, 2.0f };
        static constexpr float morph[] = { 0.15f, 0.35f, 0.58f, 0.82f };
        static constexpr float octave[] = { 0.0f, -1.0f, 0.0f, 1.0f };
        static constexpr float glide[] = { 8.0f, 24.0f, 55.0f, 95.0f };
        static constexpr float detune[] = { 3.0f, 10.0f, 18.0f, 28.0f };
        static constexpr float subLevel[] = { 0.15f, 0.35f, 0.55f, 0.72f };
        static constexpr float mix[] = { 0.62f, 0.72f, 0.82f, 0.88f };
        add ("WAVEFORM", waveform[index]); add ("WAVE_MORPH", morph[index]);
        add ("OCTAVE", octave[index]); add ("GLIDE", glide[index]);
        add ("DETUNE", detune[index]); add ("SUBLEVEL", subLevel[index]);
        add ("MIX", mix[index]);
    }
    else if (category.contains ("DRUM ENHANCER"))
    {
        static constexpr float crossover[] = { 90.0f, 130.0f, 190.0f, 280.0f };
        static constexpr float lowDrive[] = { 0.20f, 0.38f, 0.56f, 0.72f };
        static constexpr float highDrive[] = { 0.18f, 0.30f, 0.48f, 0.62f };
        add ("CROSSOVER", crossover[index]); add ("LOWDRIVE", lowDrive[index]); add ("HIGHDRIVE", highDrive[index]);
    }
    else if (category.contains ("PHASE ALIGN"))
    {
        static constexpr float delay[] = { 0.0f, 0.15f, 0.35f, 0.60f };
        static constexpr float rotation[] = { 0.0f, 0.15f, 0.35f, 0.55f };
        add ("DELAY", delay[index]); add ("ROTATIONFREQ", rotation[index]);
    }

    return values;
}
}
