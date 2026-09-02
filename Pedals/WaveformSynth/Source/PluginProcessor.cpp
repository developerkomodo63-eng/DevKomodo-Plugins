#include "PluginProcessor.h"
#include "DevKomodoUI.h"

namespace
{
    float midiToFrequency (int midiNote) noexcept
    {
        return 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f);
    }

    juce::String normalizePresetName (const juce::String& name)
    {
        auto trimmed = name.trim();
        trimmed = trimmed.replaceCharacter ('\t', ' ');
        trimmed = trimmed.replaceCharacter ('\n', ' ');
        trimmed = trimmed.replaceCharacter ('\r', ' ');
        while (trimmed.contains ("  "))
            trimmed = trimmed.replace ("  ", " ");

        return trimmed;
    }

    juce::StringArray getKnownParameterIds()
    {
        static const juce::StringArray ids {
            "PRESET", "OSC_A_BANK", "OSC_A_WAVEFORM", "OSC_A_POSITION", "OSC_A_PHASE",
            "OSC_B_BANK", "OSC_B_WAVEFORM", "OSC_B_POSITION", "OSC_B_PHASE", "POSITION",
            "WAVE_MORPH", "DETUNE", "DRIFT", "OSC_SYNC", "VIBRATO", "MOD_DEPTH",
            "FILTER_MODE", "VOICE_MODE", "GLIDE", "UNISON", "SUB", "MIX", "SPREAD",
            "WIDTH", "AIR", "NOISE", "WARMTH", "CHARACTER", "ATTACK", "DECAY",
            "SUSTAIN", "RELEASE", "DRIVE", "CUTOFF", "FILTER_DRIVE", "RESONANCE", "LEVEL"
        };

        return ids;
    }

    int getChoiceIndex (const juce::AudioProcessorValueTreeState& state, const juce::String& id, int maximumIndex) noexcept
    {
        // getRawParameterValue() for an AudioParameterChoice already returns the
        // real choice index (0..numChoices-1), NOT a normalised 0-1 value, so it
        // must not be multiplied by maximumIndex again -- doing so was scrambling
        // every combo box (osc bank/waveform, filter mode, voice mode).
        if (const auto* raw = state.getRawParameterValue (id))
            return juce::jlimit (0, maximumIndex, juce::roundToInt (raw->load()));

        return 0;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout WaveformSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "PRESET", 1 }, "Preset", juce::StringArray { "Sub Bass", "Analog Lead", "Soft Pad", "Pluck", "Dirty Saw", "Hybrid Texture", "Glass Arp", "Bass Drone" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "OSC_A_BANK", 1 }, "Osc A Bank", juce::StringArray { "Classic", "Analog", "Digital", "Hybrid" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "OSC_A_WAVEFORM", 1 }, "Osc A Waveform", juce::StringArray { "Sine", "Saw", "Square", "Triangle", "Pulse", "Soft Saw", "Harmonic", "Folded", "Ramp", "PWM" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "OSC_A_POSITION", 1 }, "Osc A Position", 0.0f, 1.0f, 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "OSC_A_PHASE", 1 }, "Osc A Phase", 0.0f, 1.0f, 0.12f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "OSC_B_BANK", 1 }, "Osc B Bank", juce::StringArray { "Classic", "Analog", "Digital", "Hybrid" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "OSC_B_WAVEFORM", 1 }, "Osc B Waveform", juce::StringArray { "Sine", "Saw", "Square", "Triangle", "Pulse", "Soft Saw", "Harmonic", "Folded", "Ramp", "PWM" }, 2));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "OSC_B_POSITION", 1 }, "Osc B Position", 0.0f, 1.0f, 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "OSC_B_PHASE", 1 }, "Osc B Phase", 0.0f, 1.0f, 0.32f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "POSITION", 1 }, "Position", 0.0f, 1.0f, 0.42f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "WAVE_MORPH", 1 }, "Wave Morph", 0.0f, 1.0f, 0.48f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DETUNE", 1 }, "Detune", 0.0f, 24.0f, 1.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIFT", 1 }, "Drift", 0.0f, 1.0f, 0.15f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "OSC_SYNC", 1 }, "Osc Sync", 0.0f, 1.0f, 0.18f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "VIBRATO", 1 }, "Vibrato", 0.0f, 1.0f, 0.14f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MOD_DEPTH", 1 }, "Mod Depth", 0.0f, 1.0f, 0.18f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "FILTER_MODE", 1 }, "Filter Mode", juce::StringArray { "LP", "BP", "HP", "Notch" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "VOICE_MODE", 1 }, "Voice Mode", juce::StringArray { "Poly", "Mono", "Legato" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "GLIDE", 1 }, "Glide", 0.0f, 200.0f, 12.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "UNISON", 1 }, "Unison", 0.0f, 1.0f, 0.18f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SUB", 1 }, "Sub", 0.0f, 1.0f, 0.28f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "MIX", 1 }, "Mix", 0.0f, 1.0f, 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SPREAD", 1 }, "Spread", 0.0f, 1.0f, 0.30f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "WIDTH", 1 }, "Width", 0.0f, 1.0f, 0.60f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "AIR", 1 }, "Air", 0.0f, 1.0f, 0.20f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "NOISE", 1 }, "Noise", 0.0f, 1.0f, 0.05f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "WARMTH", 1 }, "Warmth", 0.0f, 1.0f, 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "CHARACTER", 1 }, "Character", 0.0f, 1.0f, 0.42f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "ATTACK", 1 }, "Attack", 0.0f, 2.0f, 0.01f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DECAY", 1 }, "Decay", 0.0f, 2.0f, 0.18f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "SUSTAIN", 1 }, "Sustain", 0.0f, 1.0f, 0.72f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "RELEASE", 1 }, "Release", 0.0f, 3.0f, 0.20f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "DRIVE", 1 }, "Drive", 0.0f, 1.0f, 0.12f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "CUTOFF", 1 }, "Cutoff", 300.0f, 15000.0f, 3200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "FILTER_DRIVE", 1 }, "Filter Drive", 0.0f, 1.0f, 0.32f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "RESONANCE", 1 }, "Resonance", 0.0f, 1.0f, 0.30f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "LEVEL", 1 }, "Level", -24.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

WaveformSynthAudioProcessor::WaveformSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
 #if ! JucePlugin_IsSynth
  .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
 #endif
  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    buildWavetables();
    customPresets = juce::ValueTree ("CustomPresets");
    applyPreset (0);
}

WaveformSynthAudioProcessor::~WaveformSynthAudioProcessor() = default;

void WaveformSynthAudioProcessor::applyPreset (int presetIndex)
{
    auto setParameter = [this] (const juce::String& id, float actualValue)
    {
        if (auto* parameter = apvts.getParameter (id))
        {
            const auto& range = parameter->getNormalisableRange();
            parameter->setValueNotifyingHost (range.convertTo0to1 (actualValue));
        }
    };

    auto setChoice = [&setParameter] (const juce::String& id, int value)
    {
        setParameter (id, (float) value);
    };

    auto setFloat = [&setParameter] (const juce::String& id, float value)
    {
        setParameter (id, value);
    };

    switch (presetIndex)
    {
        case 0:
            setChoice ("PRESET", 0);
            setChoice ("OSC_A_BANK", 0);
            setChoice ("OSC_A_WAVEFORM", 2);
            setFloat ("OSC_A_POSITION", 0.30f);
            setFloat ("OSC_A_PHASE", 0.12f);
            setChoice ("OSC_B_BANK", 0);
            setChoice ("OSC_B_WAVEFORM", 1);
            setFloat ("OSC_B_POSITION", 0.55f);
            setFloat ("OSC_B_PHASE", 0.32f);
            setFloat ("POSITION", 0.30f);
            setFloat ("WAVE_MORPH", 0.44f);
            setFloat ("DETUNE", 1.2f);
            setFloat ("DRIFT", 0.12f);
            setFloat ("OSC_SYNC", 0.20f);
            setFloat ("VIBRATO", 0.10f);
            setFloat ("MOD_DEPTH", 0.18f);
            setChoice ("FILTER_MODE", 0);
            setChoice ("VOICE_MODE", 0);
            setFloat ("GLIDE", 8.0f);
            setFloat ("UNISON", 0.12f);
            setFloat ("SUB", 0.90f);
            setFloat ("MIX", 0.68f);
            setFloat ("SPREAD", 0.25f);
            setFloat ("WIDTH", 0.52f);
            setFloat ("AIR", 0.18f);
            setFloat ("NOISE", 0.04f);
            setFloat ("WARMTH", 0.48f);
            setFloat ("CHARACTER", 0.36f);
            setFloat ("ATTACK", 0.01f);
            setFloat ("DECAY", 0.14f);
            setFloat ("SUSTAIN", 0.75f);
            setFloat ("RELEASE", 0.18f);
            setFloat ("DRIVE", 0.20f);
            setFloat ("CUTOFF", 1800.0f);
            setFloat ("FILTER_DRIVE", 0.28f);
            setFloat ("RESONANCE", 0.32f);
            setFloat ("LEVEL", 0.0f);
            break;

        case 1:
            setChoice ("PRESET", 1);
            setChoice ("OSC_A_BANK", 1);
            setChoice ("OSC_A_WAVEFORM", 2);
            setFloat ("OSC_A_POSITION", 0.42f);
            setFloat ("OSC_A_PHASE", 0.18f);
            setChoice ("OSC_B_BANK", 1);
            setChoice ("OSC_B_WAVEFORM", 5);
            setFloat ("OSC_B_POSITION", 0.62f);
            setFloat ("OSC_B_PHASE", 0.38f);
            setFloat ("POSITION", 0.46f);
            setFloat ("WAVE_MORPH", 0.58f);
            setFloat ("DETUNE", 2.2f);
            setFloat ("DRIFT", 0.22f);
            setFloat ("OSC_SYNC", 0.30f);
            setFloat ("VIBRATO", 0.20f);
            setFloat ("MOD_DEPTH", 0.28f);
            setChoice ("FILTER_MODE", 1);
            setChoice ("VOICE_MODE", 0);
            setFloat ("GLIDE", 18.0f);
            setFloat ("UNISON", 0.22f);
            setFloat ("SUB", 0.20f);
            setFloat ("MIX", 0.58f);
            setFloat ("SPREAD", 0.42f);
            setFloat ("WIDTH", 0.72f);
            setFloat ("AIR", 0.26f);
            setFloat ("NOISE", 0.02f);
            setFloat ("WARMTH", 0.60f);
            setFloat ("CHARACTER", 0.54f);
            setFloat ("ATTACK", 0.01f);
            setFloat ("DECAY", 0.22f);
            setFloat ("SUSTAIN", 0.64f);
            setFloat ("RELEASE", 0.22f);
            setFloat ("DRIVE", 0.28f);
            setFloat ("CUTOFF", 4200.0f);
            setFloat ("FILTER_DRIVE", 0.38f);
            setFloat ("RESONANCE", 0.28f);
            setFloat ("LEVEL", 1.0f);
            break;

        case 2:
            setChoice ("PRESET", 2);
            setChoice ("OSC_A_BANK", 3);
            setChoice ("OSC_A_WAVEFORM", 3);
            setFloat ("OSC_A_POSITION", 0.50f);
            setFloat ("OSC_A_PHASE", 0.22f);
            setChoice ("OSC_B_BANK", 2);
            setChoice ("OSC_B_WAVEFORM", 0);
            setFloat ("OSC_B_POSITION", 0.60f);
            setFloat ("OSC_B_PHASE", 0.40f);
            setFloat ("POSITION", 0.58f);
            setFloat ("WAVE_MORPH", 0.36f);
            setFloat ("DETUNE", 2.8f);
            setFloat ("DRIFT", 0.18f);
            setFloat ("OSC_SYNC", 0.14f);
            setFloat ("VIBRATO", 0.12f);
            setFloat ("MOD_DEPTH", 0.22f);
            setChoice ("FILTER_MODE", 2);
            setChoice ("VOICE_MODE", 2);
            setFloat ("GLIDE", 24.0f);
            setFloat ("UNISON", 0.10f);
            setFloat ("SUB", 0.50f);
            setFloat ("MIX", 0.48f);
            setFloat ("SPREAD", 0.35f);
            setFloat ("WIDTH", 0.46f);
            setFloat ("AIR", 0.14f);
            setFloat ("NOISE", 0.08f);
            setFloat ("WARMTH", 0.84f);
            setFloat ("CHARACTER", 0.48f);
            setFloat ("ATTACK", 0.12f);
            setFloat ("DECAY", 0.38f);
            setFloat ("SUSTAIN", 0.82f);
            setFloat ("RELEASE", 0.42f);
            setFloat ("DRIVE", 0.10f);
            setFloat ("CUTOFF", 2600.0f);
            setFloat ("FILTER_DRIVE", 0.22f);
            setFloat ("RESONANCE", 0.26f);
            setFloat ("LEVEL", 0.0f);
            break;

        case 3:
            setChoice ("PRESET", 3);
            setChoice ("OSC_A_BANK", 2);
            setChoice ("OSC_A_WAVEFORM", 5);
            setFloat ("OSC_A_POSITION", 0.38f);
            setFloat ("OSC_A_PHASE", 0.14f);
            setChoice ("OSC_B_BANK", 2);
            setChoice ("OSC_B_WAVEFORM", 3);
            setFloat ("OSC_B_POSITION", 0.72f);
            setFloat ("OSC_B_PHASE", 0.46f);
            setFloat ("POSITION", 0.36f);
            setFloat ("WAVE_MORPH", 0.66f);
            setFloat ("DETUNE", 3.8f);
            setFloat ("DRIFT", 0.16f);
            setFloat ("OSC_SYNC", 0.10f);
            setFloat ("VIBRATO", 0.18f);
            setFloat ("MOD_DEPTH", 0.26f);
            setChoice ("FILTER_MODE", 0);
            setChoice ("VOICE_MODE", 0);
            setFloat ("GLIDE", 12.0f);
            setFloat ("UNISON", 0.20f);
            setFloat ("SUB", 0.15f);
            setFloat ("MIX", 0.62f);
            setFloat ("SPREAD", 0.50f);
            setFloat ("WIDTH", 0.66f);
            setFloat ("AIR", 0.22f);
            setFloat ("NOISE", 0.05f);
            setFloat ("WARMTH", 0.42f);
            setFloat ("CHARACTER", 0.40f);
            setFloat ("ATTACK", 0.01f);
            setFloat ("DECAY", 0.18f);
            setFloat ("SUSTAIN", 0.58f);
            setFloat ("RELEASE", 0.16f);
            setFloat ("DRIVE", 0.34f);
            setFloat ("CUTOFF", 5200.0f);
            setFloat ("RESONANCE", 0.40f);
            setFloat ("LEVEL", 1.0f);
            break;

        case 4:
            setChoice ("PRESET", 4);
            setChoice ("OSC_A_BANK", 1);
            setChoice ("OSC_A_WAVEFORM", 1);
            setFloat ("OSC_A_POSITION", 0.46f);
            setFloat ("OSC_A_PHASE", 0.20f);
            setChoice ("OSC_B_BANK", 1);
            setChoice ("OSC_B_WAVEFORM", 2);
            setFloat ("OSC_B_POSITION", 0.78f);
            setFloat ("OSC_B_PHASE", 0.58f);
            setFloat ("POSITION", 0.52f);
            setFloat ("WAVE_MORPH", 0.72f);
            setFloat ("DETUNE", 5.5f);
            setFloat ("DRIFT", 0.24f);
            setFloat ("OSC_SYNC", 0.38f);
            setFloat ("VIBRATO", 0.26f);
            setFloat ("MOD_DEPTH", 0.34f);
            setChoice ("FILTER_MODE", 3);
            setChoice ("VOICE_MODE", 1);
            setFloat ("GLIDE", 16.0f);
            setFloat ("UNISON", 0.36f);
            setFloat ("SUB", 0.18f);
            setFloat ("MIX", 0.76f);
            setFloat ("SPREAD", 0.68f);
            setFloat ("WIDTH", 0.80f);
            setFloat ("AIR", 0.30f);
            setFloat ("NOISE", 0.06f);
            setFloat ("WARMTH", 0.56f);
            setFloat ("CHARACTER", 0.66f);
            setFloat ("ATTACK", 0.01f);
            setFloat ("DECAY", 0.16f);
            setFloat ("SUSTAIN", 0.60f);
            setFloat ("RELEASE", 0.18f);
            setFloat ("DRIVE", 0.52f);
            setFloat ("CUTOFF", 6400.0f);
            setFloat ("FILTER_DRIVE", 0.60f);
            setFloat ("RESONANCE", 0.46f);
            setFloat ("LEVEL", 0.8f);
            break;

        case 5:
            setChoice ("PRESET", 5);
            setChoice ("OSC_A_BANK", 3);
            setChoice ("OSC_A_WAVEFORM", 1);
            setFloat ("OSC_A_POSITION", 0.54f);
            setFloat ("OSC_A_PHASE", 0.28f);
            setChoice ("OSC_B_BANK", 3);
            setChoice ("OSC_B_WAVEFORM", 4);
            setFloat ("OSC_B_POSITION", 0.67f);
            setFloat ("OSC_B_PHASE", 0.44f);
            setFloat ("POSITION", 0.62f);
            setFloat ("WAVE_MORPH", 0.52f);
            setFloat ("DETUNE", 4.4f);
            setFloat ("DRIFT", 0.20f);
            setFloat ("OSC_SYNC", 0.26f);
            setFloat ("VIBRATO", 0.16f);
            setFloat ("MOD_DEPTH", 0.24f);
            setChoice ("FILTER_MODE", 1);
            setChoice ("VOICE_MODE", 0);
            setFloat ("GLIDE", 20.0f);
            setFloat ("UNISON", 0.26f);
            setFloat ("SUB", 0.28f);
            setFloat ("MIX", 0.66f);
            setFloat ("SPREAD", 0.58f);
            setFloat ("WIDTH", 0.62f);
            setFloat ("AIR", 0.24f);
            setFloat ("NOISE", 0.10f);
            setFloat ("WARMTH", 0.70f);
            setFloat ("CHARACTER", 0.58f);
            setFloat ("ATTACK", 0.02f);
            setFloat ("DECAY", 0.30f);
            setFloat ("SUSTAIN", 0.68f);
            setFloat ("RELEASE", 0.24f);
            setFloat ("DRIVE", 0.38f);
            setFloat ("CUTOFF", 5100.0f);
            setFloat ("FILTER_DRIVE", 0.46f);
            setFloat ("RESONANCE", 0.44f);
            setFloat ("LEVEL", 0.6f);
            break;

        case 6:
            setChoice ("PRESET", 6);
            setChoice ("OSC_A_BANK", 2);
            setChoice ("OSC_A_WAVEFORM", 6);
            setFloat ("OSC_A_POSITION", 0.46f);
            setFloat ("OSC_A_PHASE", 0.18f);
            setChoice ("OSC_B_BANK", 3);
            setChoice ("OSC_B_WAVEFORM", 7);
            setFloat ("OSC_B_POSITION", 0.60f);
            setFloat ("OSC_B_PHASE", 0.35f);
            setFloat ("POSITION", 0.52f);
            setFloat ("WAVE_MORPH", 0.24f);
            setFloat ("DETUNE", 2.8f);
            setFloat ("DRIFT", 0.16f);
            setFloat ("OSC_SYNC", 0.12f);
            setFloat ("VIBRATO", 0.14f);
            setFloat ("MOD_DEPTH", 0.22f);
            setChoice ("FILTER_MODE", 1);
            setChoice ("VOICE_MODE", 0);
            setFloat ("GLIDE", 9.0f);
            setFloat ("UNISON", 0.18f);
            setFloat ("SUB", 0.12f);
            setFloat ("MIX", 0.52f);
            setFloat ("SPREAD", 0.48f);
            setFloat ("WIDTH", 0.56f);
            setFloat ("AIR", 0.34f);
            setFloat ("NOISE", 0.06f);
            setFloat ("WARMTH", 0.68f);
            setFloat ("CHARACTER", 0.52f);
            setFloat ("ATTACK", 0.01f);
            setFloat ("DECAY", 0.20f);
            setFloat ("SUSTAIN", 0.62f);
            setFloat ("RELEASE", 0.18f);
            setFloat ("DRIVE", 0.18f);
            setFloat ("CUTOFF", 5600.0f);
            setFloat ("FILTER_DRIVE", 0.34f);
            setFloat ("RESONANCE", 0.38f);
            setFloat ("LEVEL", 0.8f);
            break;

        case 7:
            setChoice ("PRESET", 7);
            setChoice ("OSC_A_BANK", 1);
            setChoice ("OSC_A_WAVEFORM", 1);
            setFloat ("OSC_A_POSITION", 0.38f);
            setFloat ("OSC_A_PHASE", 0.26f);
            setChoice ("OSC_B_BANK", 0);
            setChoice ("OSC_B_WAVEFORM", 3);
            setFloat ("OSC_B_POSITION", 0.72f);
            setFloat ("OSC_B_PHASE", 0.52f);
            setFloat ("POSITION", 0.64f);
            setFloat ("WAVE_MORPH", 0.62f);
            setFloat ("DETUNE", 0.8f);
            setFloat ("DRIFT", 0.28f);
            setFloat ("OSC_SYNC", 0.42f);
            setFloat ("VIBRATO", 0.22f);
            setFloat ("MOD_DEPTH", 0.26f);
            setChoice ("FILTER_MODE", 0);
            setChoice ("VOICE_MODE", 2);
            setFloat ("GLIDE", 14.0f);
            setFloat ("UNISON", 0.26f);
            setFloat ("SUB", 0.72f);
            setFloat ("MIX", 0.58f);
            setFloat ("SPREAD", 0.32f);
            setFloat ("WIDTH", 0.48f);
            setFloat ("AIR", 0.16f);
            setFloat ("NOISE", 0.04f);
            setFloat ("WARMTH", 0.60f);
            setFloat ("CHARACTER", 0.44f);
            setFloat ("ATTACK", 0.02f);
            setFloat ("DECAY", 0.22f);
            setFloat ("SUSTAIN", 0.74f);
            setFloat ("RELEASE", 0.38f);
            setFloat ("DRIVE", 0.28f);
            setFloat ("CUTOFF", 1800.0f);
            setFloat ("RESONANCE", 0.26f);
            setFloat ("LEVEL", 0.7f);
            break;

        default:
            applyPreset (0);
            break;
    }
}

void WaveformSynthAudioProcessor::buildWavetables()
{
    for (int bank = 0; bank < wavetableBanks; ++bank)
    {
        const float bankBias = (float) bank * 0.22f;

        for (int i = 0; i < tableSize; ++i)
        {
            const float phase = (float) i / (float) tableSize;
            const float angle = phase * juce::MathConstants<float>::twoPi;
            const float harmonicBoost = 1.0f + (float) bank * 0.18f;

            const float sine = std::sin (angle + bankBias);
            const float saw = 2.0f * phase - 1.0f + 0.10f * std::sin (angle * 2.0f + bankBias);
            const float square = (phase < 0.5f + 0.05f * (float) bank ? 1.0f : -1.0f) * harmonicBoost;
            const float triangle = (1.0f - std::abs (2.0f * phase - 1.0f) * 2.0f) * (1.0f + 0.12f * (float) bank);
            const float pulse = (phase < 0.25f + 0.02f * (float) bank ? 1.0f : -1.0f) * (1.0f + 0.08f * (float) bank);
            const float softSaw = 0.7f * saw + 0.3f * sine + 0.08f * std::sin (angle * 3.0f + bankBias);
            const float harmonic = 0.65f * sine + 0.35f * std::sin (angle * 3.0f + bankBias);
            const float folded = std::sin (angle + bankBias) * (1.0f - 0.35f * std::abs (std::sin (angle * 2.0f + bankBias)));
            const float ramp = 1.0f - (phase * 2.0f);
            const float pwm = (phase < 0.35f + 0.10f * std::sin (angle * 2.0f + bankBias) ? 1.0f : -1.0f);
            const float richWave = 0.5f * saw + 0.25f * square + 0.25f * sine;

            wavetableBank[bank][0][i] = sine;
            wavetableBank[bank][1][i] = saw;
            wavetableBank[bank][2][i] = square;
            wavetableBank[bank][3][i] = triangle;
            wavetableBank[bank][4][i] = pulse;
            wavetableBank[bank][5][i] = softSaw;
            wavetableBank[bank][6][i] = harmonic;
            wavetableBank[bank][7][i] = folded;
            wavetableBank[bank][8][i] = ramp;
            wavetableBank[bank][9][i] = pwm * (1.0f + 0.12f * (float) bank) + richWave * 0.18f;
        }
    }
}

float WaveformSynthAudioProcessor::wavetableSample (float phase, float morph, float detuneOffset, int bankIndex) const
{
    const int bank = juce::jlimit (0, wavetableBanks - 1, bankIndex);
    const int waveform = juce::jlimit (0, wavetableCount - 1, (int) std::lround (morph * (float) (wavetableCount - 1)));
    // "phase" is a CYCLIC value (0..1 repeating), not a bounded one -- but
    // every call site adds an offset to it (stereo spread, osc phase
    // shift, detune), which routinely pushes the sum past 1.0 or below
    // 0.0. Clamping that (the previous behaviour) made the oscillator
    // read the very last table sample over and over for a large chunk of
    // every cycle instead of wrapping around smoothly -- that's what was
    // making this sound stuck/distorted rather than like a clean tone.
    // std::floor here also handles negative offsets correctly (e.g.
    // rawPhase = -0.3 wraps to 0.7), unlike a plain fmod.
    const float rawPhase = phase + detuneOffset;
    const float normalizedPhase = rawPhase - std::floor (rawPhase);
    const float indexF = normalizedPhase * (float) (tableSize - 1);
    const int indexA = (int) indexF;
    const int indexB = (indexA + 1) % tableSize;
    const float frac = indexF - (float) indexA;

    const float current = wavetableBank[bank][(size_t) waveform][(size_t) indexA];
    const float next = wavetableBank[bank][(size_t) waveform][(size_t) indexB];
    return current * (1.0f - frac) + next * frac;
}

void WaveformSynthAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = newSampleRate;

    clearHeldNotes();

    for (auto& voice : voices)
    {
        voice.active = false;
        voice.releasing = false;
        voice.phase = 0.0f;
        voice.amplitude = 0.0f;
        voice.env = 0.0f;
        voice.lastSample = 0.0f;
    }

    levelSmoothed.reset (newSampleRate, 0.02);
    positionSmoothed.reset (newSampleRate, 0.02);
    detuneSmoothed.reset (newSampleRate, 0.02);
    levelSmoothed.setCurrentAndTargetValue (0.0f);
    positionSmoothed.setCurrentAndTargetValue (0.35f);
    detuneSmoothed.setCurrentAndTargetValue (2.0f);
}

void WaveformSynthAudioProcessor::releaseResources() {}

bool WaveformSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
#endif
}

int WaveformSynthAudioProcessor::findReusableVoice (int midiNote) const
{
    juce::ignoreUnused (midiNote);
    const int voiceMode = getChoiceIndex (apvts, "VOICE_MODE", 2);

    if (voiceMode == 1 || voiceMode == 2)
        return 0;

    for (int i = 0; i < (int) voices.size(); ++i)
        if (! voices[(size_t) i].active)
            return i;

    for (int i = 0; i < (int) voices.size(); ++i)
        if (voices[(size_t) i].releasing)
            return i;

    return 0;
}

void WaveformSynthAudioProcessor::clearHeldNotes()
{
    numHeldNotes = 0;
    heldNotes.fill (-1);
    heldVelocities.fill (0.0f);
}

void WaveformSynthAudioProcessor::addHeldNote (int midiNote, float velocity)
{
    removeHeldNote (midiNote);
    if (numHeldNotes >= (int) heldNotes.size())
        return;

    heldNotes[(size_t) numHeldNotes] = midiNote;
    heldVelocities[(size_t) numHeldNotes] = velocity;
    ++numHeldNotes;
}

void WaveformSynthAudioProcessor::removeHeldNote (int midiNote)
{
    for (int i = 0; i < numHeldNotes; ++i)
    {
        if (heldNotes[(size_t) i] == midiNote)
        {
            for (int j = i; j < numHeldNotes - 1; ++j)
            {
                heldNotes[(size_t) j] = heldNotes[(size_t) (j + 1)];
                heldVelocities[(size_t) j] = heldVelocities[(size_t) (j + 1)];
            }
            --numHeldNotes;
            heldNotes[(size_t) numHeldNotes] = -1;
            heldVelocities[(size_t) numHeldNotes] = 0.0f;
            return;
        }
    }
}

int WaveformSynthAudioProcessor::getLatestHeldNote() const
{
    return numHeldNotes > 0 ? heldNotes[(size_t) (numHeldNotes - 1)] : -1;
}

void WaveformSynthAudioProcessor::noteOn (int midiNote, float velocity)
{
    const int voiceMode = getChoiceIndex (apvts, "VOICE_MODE", 2);
    const float safeVelocity = juce::jlimit (0.0f, 1.0f, velocity);

    if (voiceMode == 1 || voiceMode == 2)
        addHeldNote (midiNote, safeVelocity);

    const int targetIndex = findReusableVoice (midiNote);
    auto& voice = voices[(size_t) targetIndex];
    const bool wasActive = voice.active && ! voice.releasing;
    const bool legato = voiceMode == 2 && wasActive;

    voice.active = true;
    voice.releasing = false;
    voice.midiNote = midiNote;
    voice.targetFrequency = midiToFrequency (midiNote);
    voice.velocity = safeVelocity;

    // Mono retriggers the envelope; Legato changes pitch without restarting it.
    if (! wasActive)
        voice.frequency = voice.targetFrequency;

    if (! legato)
    {
        voice.phase = 0.0f;
        voice.env = 0.0001f;
        voice.amplitude = 0.0f;
        voice.lastSample = 0.0f;
        voice.filterStateL = 0.0f;
        voice.filterStateR = 0.0f;
    }

    voice.attackTime = juce::jlimit (0.001f, 2.0f, apvts.getRawParameterValue ("ATTACK")->load());
    voice.decayTime = juce::jlimit (0.001f, 2.0f, apvts.getRawParameterValue ("DECAY")->load());
    voice.sustainLevel = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("SUSTAIN")->load());
    voice.releaseTime = juce::jlimit (0.001f, 3.0f, apvts.getRawParameterValue ("RELEASE")->load());
}

void WaveformSynthAudioProcessor::noteOff (int midiNote)
{
    const int voiceMode = getChoiceIndex (apvts, "VOICE_MODE", 2);

    if (voiceMode == 1 || voiceMode == 2)
    {
        removeHeldNote (midiNote);
        auto& voice = voices[0];

        if (const int latest = getLatestHeldNote(); latest >= 0)
        {
            voice.midiNote = latest;
            voice.targetFrequency = midiToFrequency (latest);
            voice.velocity = heldVelocities[(size_t) (numHeldNotes - 1)];
            voice.releasing = false;
            return;
        }

        if (voice.active)
            voice.releasing = true;
        return;
    }

    for (auto& voice : voices)
        if (voice.active && voice.midiNote == midiNote)
            voice.releasing = true;
}

void WaveformSynthAudioProcessor::saveCurrentPresetAsUserPreset (const juce::String& name)
{
    const auto trimmedName = normalizePresetName (name);
    if (trimmedName.isEmpty())
        return;

    if (! customPresets.isValid())
        customPresets = juce::ValueTree ("CustomPresets");

    juce::ValueTree presetTree ("Preset");
    presetTree.setProperty ("name", trimmedName, nullptr);

    for (const auto& parameterId : getKnownParameterIds())
        if (auto* parameter = apvts.getParameter (parameterId))
            presetTree.setProperty (parameterId, parameter->getValue(), nullptr);

    for (int i = customPresets.getNumChildren() - 1; i >= 0; --i)
    {
        auto child = customPresets.getChild (i);
        if (child.getProperty ("name").toString() == trimmedName)
        {
            customPresets.removeChild (i, nullptr);
            break;
        }
    }

    customPresets.appendChild (presetTree, nullptr);
}

void WaveformSynthAudioProcessor::loadUserPreset (const juce::String& name)
{
    const auto trimmedName = normalizePresetName (name);
    if (trimmedName.isEmpty())
        return;

    for (int i = 0; i < customPresets.getNumChildren(); ++i)
    {
        auto child = customPresets.getChild (i);
        if (child.getProperty ("name").toString() == trimmedName)
        {
            for (const auto& parameterId : getKnownParameterIds())
            {
                if (child.hasProperty (parameterId))
                    if (auto* parameter = apvts.getParameter (parameterId))
                        parameter->setValueNotifyingHost ((float) child.getProperty (parameterId));
            }
            return;
        }
    }
}

void WaveformSynthAudioProcessor::deleteUserPreset (const juce::String& name)
{
    const auto trimmedName = normalizePresetName (name);
    if (trimmedName.isEmpty())
        return;

    for (int i = customPresets.getNumChildren() - 1; i >= 0; --i)
    {
        auto child = customPresets.getChild (i);
        if (child.getProperty ("name").toString() == trimmedName)
        {
            customPresets.removeChild (i, nullptr);
            return;
        }
    }
}

juce::StringArray WaveformSynthAudioProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& child : customPresets)
    {
        const auto presetName = normalizePresetName (child.getProperty ("name").toString());
        if (! presetName.isEmpty())
            names.add (presetName);
    }

    names.sortNatural();
    return names;
}

void WaveformSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
#if defined (DEVKOMODO_DEMO_BUILD)
    if (devkomodo::demoExpired (getSampleRate(), buffer.getNumSamples()))
    {
        buffer.clear();
        return;
    }
#endif

    buffer.clear();

    for (const auto& event : midiMessages)
    {
        const auto message = event.getMessage();
        if (message.isNoteOn())
            noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            noteOff (message.getNoteNumber());
        else if (message.isController() && message.getControllerNumber() == 123)
        {
            clearHeldNotes();
            for (auto& voice : voices)
                voice.releasing = voice.active;
        }
    }

    const int oscABank = getChoiceIndex (apvts, "OSC_A_BANK", 3);
    const int oscAWaveform = getChoiceIndex (apvts, "OSC_A_WAVEFORM", 9);
    const float oscAPosition = apvts.getRawParameterValue ("OSC_A_POSITION")->load();
    const float oscAPhase = apvts.getRawParameterValue ("OSC_A_PHASE")->load();
    const int oscBBank = getChoiceIndex (apvts, "OSC_B_BANK", 3);
    const int oscBWaveform = getChoiceIndex (apvts, "OSC_B_WAVEFORM", 9);
    const float oscBPosition = apvts.getRawParameterValue ("OSC_B_POSITION")->load();
    const float oscBPhase = apvts.getRawParameterValue ("OSC_B_PHASE")->load();
    const float position = apvts.getRawParameterValue ("POSITION")->load();
    const float waveMorph = apvts.getRawParameterValue ("WAVE_MORPH")->load();
    const float detune = apvts.getRawParameterValue ("DETUNE")->load();
    const float drift = apvts.getRawParameterValue ("DRIFT")->load();
    const float oscSync = apvts.getRawParameterValue ("OSC_SYNC")->load();
    const float vibrato = apvts.getRawParameterValue ("VIBRATO")->load();
    const float modDepth = apvts.getRawParameterValue ("MOD_DEPTH")->load();
    const int filterMode = getChoiceIndex (apvts, "FILTER_MODE", 3);
    const float glideMs = apvts.getRawParameterValue ("GLIDE")->load();
    const float unisonDepth = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("UNISON")->load());
    const float subLevel = apvts.getRawParameterValue ("SUB")->load();
    const float mix = apvts.getRawParameterValue ("MIX")->load();
    const float spread = apvts.getRawParameterValue ("SPREAD")->load();
    const float width = apvts.getRawParameterValue ("WIDTH")->load();
    const float air = apvts.getRawParameterValue ("AIR")->load();
    const float noiseAmount = apvts.getRawParameterValue ("NOISE")->load();
    const float warmth = apvts.getRawParameterValue ("WARMTH")->load();
    const float character = apvts.getRawParameterValue ("CHARACTER")->load();
    const float driveAmount = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("DRIVE")->load());
    const float cutoffHz = apvts.getRawParameterValue ("CUTOFF")->load();
    const float filterDrive = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("FILTER_DRIVE")->load());
    const float resonance = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("RESONANCE")->load());
    const float outputLevel = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("LEVEL")->load());

    const float attackTime = juce::jmax (0.001f, apvts.getRawParameterValue ("ATTACK")->load());
    const float decayTime = juce::jmax (0.001f, apvts.getRawParameterValue ("DECAY")->load());
    const float releaseTime = juce::jmax (0.001f, apvts.getRawParameterValue ("RELEASE")->load());
    const float sustainLevel = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("SUSTAIN")->load());
    const float glideSeconds = juce::jmax (0.0001f, glideMs / 1000.0f);
    const float glideCoeff = glideMs <= 0.01f ? 1.0f : 1.0f - std::exp (-1.0f / (glideSeconds * (float) sampleRate));
    const float attackCoeff = 1.0f - std::exp (-1.0f / (attackTime * (float) sampleRate));
    const float decayCoeff = 1.0f - std::exp (-1.0f / (decayTime * (float) sampleRate));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / (releaseTime * (float) sampleRate));

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;

        for (auto& voice : voices)
        {
            if (! voice.active)
                continue;

            voice.frequency += (voice.targetFrequency - voice.frequency) * glideCoeff;
            const float detuneSemitones = detune * 0.5f;
            const float driftSemitones = drift * 0.12f;
            const float vibratoSemitones = vibrato * 0.018f;
            const float modulationSemitones = modDepth * 0.025f;
            const float lfoPhase = (float) sample * 0.013f + voice.phase * 4.0f;
            const float driftedDetune = (detuneSemitones / 12.0f) + std::sin (lfoPhase) * (driftSemitones / 12.0f);
            const float modulation = std::sin ((float) sample * 0.0038f + voice.midiNote * 0.07f + voice.phase * 6.0f)
                                   * (vibratoSemitones + modulationSemitones);
            const float freq = juce::jmax (1.0f, voice.frequency * std::pow (2.0f, driftedDetune + modulation));
            voice.phase += freq / (float) sampleRate;
            voice.phase -= std::floor (voice.phase);

            const float globalMorph = juce::jlimit (0.0f, 1.0f, position + std::sin (voice.phase * juce::MathConstants<float>::twoPi) * 0.10f);
            const float morphBias = (waveMorph - 0.5f) * 0.8f;
            const float oscAMorph = juce::jlimit (0.0f, 1.0f, oscAPosition + std::sin (voice.phase * juce::MathConstants<float>::twoPi + 0.5f) * 0.14f + morphBias);
            const float oscBMorph = juce::jlimit (0.0f, 1.0f, oscBPosition + std::sin (voice.phase * juce::MathConstants<float>::twoPi + 1.4f) * 0.14f - morphBias);
            const float oscAIndex = juce::jlimit (0.0f, (float) wavetableCount - 1.0f,
                                                  (float) oscAWaveform + globalMorph * 0.35f + oscAMorph * 0.40f + waveMorph * 0.26f);
            const float oscBIndex = juce::jlimit (0.0f, (float) wavetableCount - 1.0f,
                                                  (float) oscBWaveform + globalMorph * 0.35f + oscBMorph * 0.40f + (1.0f - waveMorph) * 0.24f);
            const float syncAmount = juce::jlimit (0.0f, 1.0f, oscSync);
            const float phaseA = voice.phase + oscAPhase * 0.75f;
            const float phaseB = voice.phase + 0.18f + oscBPhase * 0.75f + syncAmount * voice.phase * 0.02f;
            const float oscA = wavetableSample (phaseA, oscAIndex / (float) (wavetableCount - 1), driftedDetune * 0.07f, oscABank) * 0.75f;
            const float oscB = wavetableSample (phaseB, oscBIndex / (float) (wavetableCount - 1), (driftedDetune + 0.8f) * 0.10f, oscBBank) * 0.72f;
            const float unison = wavetableSample (voice.phase + 0.15f + oscAPhase * 0.75f,
                                                  oscAIndex / (float) (wavetableCount - 1),
                                                  (driftedDetune + 0.5f) * 0.10f, oscABank) * (0.30f * unisonDepth);
            const float sub = std::sin (voice.phase * juce::MathConstants<float>::twoPi * 0.5f) * (0.42f * subLevel);
            const float noise = std::sin (voice.phase * 131.0f + (float) sample * 0.31f) * noiseAmount * 0.22f;
            const float baseOsc = oscA * (1.0f - mix) + oscB * mix + unison + sub + noise;

            if (voice.releasing)
            {
                voice.env = juce::jmax (0.0f, voice.env - releaseCoeff);
                if (voice.env <= 0.0001f)
                {
                    voice.active = false;
                    voice.releasing = false;
                    voice.env = 0.0f;
                    continue;
                }
            }
            else if (voice.env < 1.0f)
                voice.env = juce::jmin (1.0f, voice.env + attackCoeff * (1.0f - voice.env));
            else
                voice.env = juce::jmax (sustainLevel, voice.env - decayCoeff * (voice.env - sustainLevel));

            const float toneDrive = 1.0f + driveAmount * 2.2f + warmth * 1.2f + unisonDepth * 0.55f + character * 0.5f;
            const float distorted = std::tanh (baseOsc * toneDrive);
            const float cutoff = juce::jlimit (100.0f, 18000.0f,
                                               cutoffHz * (0.72f + position * 1.10f + warmth * 0.22f + character * 0.18f + air * 0.12f)
                                               * (0.80f + voice.env * 0.55f));
            const float filterCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
            const float filterGain = 1.0f + filterDrive * 0.45f;

            auto processFilter = [&] (float input, float& state) -> float
            {
                float filtered = input;
                switch (filterMode)
                {
                    case 1: // BP
                    {
                        const float hp = input - state;
                        state += filterCoeff * hp;
                        filtered = state;
                        break;
                    }
                    case 2: // HP
                        state += filterCoeff * (input - state);
                        filtered = input - state;
                        break;
                    case 3: // Notch
                        state += filterCoeff * (input - state);
                        filtered = input - resonance * state;
                        break;
                    default: // LP
                        state += filterCoeff * (input - state);
                        filtered = state;
                        break;
                }
                return std::tanh (filtered * filterGain * (1.0f + filterDrive * 0.35f));
            };

            const float pan = juce::jlimit (-1.0f, 1.0f, std::sin (voice.phase * juce::MathConstants<float>::twoPi) * spread);
            const float leftGain = std::sqrt (0.5f * (1.0f - pan));
            const float rightGain = std::sqrt (0.5f * (1.0f + pan));
            const float filteredL = processFilter (distorted, voice.filterStateL);
            const float filteredR = processFilter (distorted * (1.0f + 0.03f * spread), voice.filterStateR);
            const float voiceGain = voice.env * (0.18f * voice.velocity);

            left += filteredL * voiceGain * leftGain;
            right += filteredR * voiceGain * rightGain;

        }

        const float mid = 0.5f * (left + right);
        const float side = 0.5f * (left - right) * (1.0f + width * 1.5f);
        left = (mid + side) * outputLevel * 0.9f;
        right = (mid - side) * outputLevel * 0.9f;

        buffer.setSample (0, sample, juce::jlimit (-1.0f, 1.0f, left));
        if (numChannels > 1)
            buffer.setSample (1, sample, juce::jlimit (-1.0f, 1.0f, right));
        for (int channel = 2; channel < numChannels; ++channel)
            buffer.setSample (channel, sample, 0.0f);
    }
}

void WaveformSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (customPresets.isValid())
        state.appendChild (customPresets, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WaveformSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        auto state = juce::ValueTree::fromXml (*xmlState);
        if (state.isValid() && state.hasType (apvts.state.getType()))
        {
            auto restoredCustomPresets = state.getChildWithName ("CustomPresets");
            if (restoredCustomPresets.isValid())
                state.removeChild (restoredCustomPresets, nullptr);

            apvts.replaceState (state);
            customPresets = restoredCustomPresets.isValid()
                                ? restoredCustomPresets
                                : juce::ValueTree ("CustomPresets");
        }
    }
}

juce::AudioProcessorEditor* WaveformSynthAudioProcessor::createEditor()
{
    return new WaveformSynthEditor (*this, apvts);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WaveformSynthAudioProcessor(); }
