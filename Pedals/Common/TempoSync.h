#pragma once
#include <JuceHeader.h>

// Shared tempo-sync helper for pedals whose character is defined by a
// periodic TIME (delay) or RATE (LFO) parameter -- Delay, TapeDelay, Chorus,
// Flanger, Phaser, Tremolo, Vibrato, Doubler. Each of those pedals adds a
// TEMPOSYNC bool and a NOTEDIV choice via addParameters() below, and calls
// resolveHz()/resolveSeconds() once per block to override the manual knob
// value with a host-tempo-locked one when sync is engaged.
namespace DevKomodoTempoSync
{
    // Standard 12-division set: straight / triplet / dotted for whole note
    // down to 1/32. Index order must stay in sync between addParameters()
    // and resolveMultiplier() below.
    inline juce::StringArray noteDivisionChoices()
    {
        return { "1/1", "1/2", "1/2T", "1/2D", "1/4", "1/4T", "1/4D",
                 "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/16D", "1/32" };
    }

    // Multiplier relative to one quarter note (e.g. 1/4 == 1.0, 1/8 == 0.5).
    inline float divisionMultiplier (int index) noexcept
    {
        static constexpr float multipliers[14] = {
            4.0f, 2.0f, 2.0f * 1.5f, 2.0f * (2.0f / 3.0f),
            1.0f, 1.0f * 1.5f, 1.0f * (2.0f / 3.0f),
            0.5f, 0.5f * 1.5f, 0.5f * (2.0f / 3.0f),
            0.25f, 0.25f * 1.5f, 0.25f * (2.0f / 3.0f),
            0.125f
        };
        return multipliers[(size_t) juce::jlimit (0, 13, index)];
    }

    // Adds "TEMPOSYNC" (bool) and "NOTEDIV" (choice) to a parameter layout
    // vector. Call from createParameterLayout() alongside the pedal's
    // existing RATE/TIME parameter.
    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                                int defaultDivisionIndex = 4 /* "1/4" */)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "TEMPOSYNC", 1 }, "Tempo Sync", false));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "NOTEDIV", 1 }, "Note Division",
            noteDivisionChoices(), defaultDivisionIndex));
    }

    // Reads the host's current BPM (falls back to 120 if the host doesn't
    // report one, e.g. some standalone/offline render contexts).
    inline float hostBpmOrFallback (juce::AudioProcessor& processor, float fallbackBpm = 120.0f) noexcept
    {
        if (auto* playHead = processor.getPlayHead())
        {
            if (auto position = playHead->getPosition())
            {
                if (auto bpm = position->getBpm())
                    if (*bpm > 1.0)
                        return (float) *bpm;
            }
        }
        return fallbackBpm;
    }

    // For RATE-style pedals (Chorus/Flanger/Phaser/Tremolo/Vibrato/Doubler):
    // returns the synced LFO rate in Hz for the given note division.
    inline float resolveHz (juce::AudioProcessor& processor, float manualRateHz,
                             bool synced, int divisionIndex,
                             float minHz, float maxHz) noexcept
    {
        if (! synced)
            return manualRateHz;
        const float bpm = hostBpmOrFallback (processor);
        const float quarterNoteSeconds = 60.0f / bpm;
        const float noteSeconds = quarterNoteSeconds * divisionMultiplier (divisionIndex);
        if (noteSeconds <= 0.0f)
            return manualRateHz;
        return juce::jlimit (minHz, maxHz, 1.0f / noteSeconds);
    }

    // For TIME-style pedals (Delay/TapeDelay): returns the synced delay time
    // in milliseconds for the given note division.
    inline float resolveMilliseconds (juce::AudioProcessor& processor, float manualTimeMs,
                                       bool synced, int divisionIndex,
                                       float minMs, float maxMs) noexcept
    {
        if (! synced)
            return manualTimeMs;
        const float bpm = hostBpmOrFallback (processor);
        const float quarterNoteSeconds = 60.0f / bpm;
        const float noteSeconds = quarterNoteSeconds * divisionMultiplier (divisionIndex);
        return juce::jlimit (minMs, maxMs, noteSeconds * 1000.0f);
    }
}
