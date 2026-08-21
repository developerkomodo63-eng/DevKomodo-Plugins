#pragma once

#include <JuceHeader.h>

namespace devkomodo
{
inline juce::String parameterTooltip (const juce::String& parameterID,
                                      const juce::String& displayName)
{
    const auto id = parameterID.toUpperCase();

    if (id == "HQ")
        return "Enables 4x oversampling around the nonlinear stage to reduce aliasing; uses more CPU.";
    if (id == "INSTRUMENT")
        return "Chooses the voicing and frequency range for guitar or bass.";
    if (id.contains ("BAND") && id.contains ("GAIN"))
        return "Sets the boost or cut for this EQ band.";
    if (id.contains ("FREQ") || id.contains ("TONE"))
        return "Sets the frequency focus or tonal balance of the effect.";
    if (id.contains ("DRIVE") || id.contains ("FUZZ") || id.contains ("DISTORT") || id == "GAIN")
        return "Controls how hard the signal drives the nonlinear stage.";
    if (id.contains ("MIX") || id == "WET")
        return "Blends the processed signal with the original input.";
    if (id.contains ("LEVEL") || id.contains ("OUTPUT") || id.contains ("MAKEUP"))
        return "Sets the output level after processing.";
    if (id.contains ("THRESH"))
        return "Sets the level at which the processor begins to act.";
    if (id.contains ("RATIO"))
        return "Sets how strongly levels above the threshold are reduced.";
    if (id.contains ("ATTACK"))
        return "Sets how quickly processing responds to a new peak.";
    if (id.contains ("RELEASE") || id.contains ("DECAY"))
        return "Sets how quickly processing returns to normal after a peak.";
    if (id.contains ("TIME") || id.contains ("DELAY"))
        return "Sets the delay or timing length of the effect.";
    if (id.contains ("RATE") || id.contains ("SPEED"))
        return "Sets the speed of the modulation or movement.";
    if (id.contains ("DEPTH") || id.contains ("AMOUNT") || id.contains ("WIDTH"))
        return "Sets the intensity or stereo spread of the effect.";
    if (id.contains ("FEEDBACK"))
        return "Feeds part of the output back into the effect for a denser response.";
    if (id.contains ("STYLE") || id.contains ("MODE") || id.contains ("VOICE") || id.contains ("CAB"))
        return "Selects the processing model used by this effect.";
    if (id.contains ("BASS") || id.contains ("LOW"))
        return "Adjusts the low-frequency emphasis.";
    if (id.contains ("MID") || id.contains ("BODY"))
        return "Adjusts the midrange weight and focus.";
    if (id.contains ("TREBLE") || id.contains ("HIGH") || id.contains ("AIR") || id.contains ("PRESENCE"))
        return "Adjusts high-frequency clarity and presence.";
    if (id.contains ("BIAS") || id.contains ("CHARACTER"))
        return "Changes the symmetry and character of the nonlinear response.";
    if (id.contains ("GATE"))
        return "Sets how strongly quiet or unwanted signal is reduced.";

    return displayName.isNotEmpty() ? displayName : parameterID;
}
}
