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
    if (id.contains ("PAN") || id.contains ("BALANCE"))
        return "Places the signal in the stereo field for a wider or more centred response.";
    if (id.contains ("CROSSOVER") || id.contains ("CENTER") || id.contains ("SPREAD"))
        return "Sets how the effect splits or distributes energy across the spectrum or stereo field.";
    if (id == "BIAS")
        return "Shifts the nonlinear response toward a softer asymmetry or a more aggressive edge; useful for warm, rounded saturation or sharper clipping.";
    if (id == "WAVE_MORPH")
        return "Morphs the oscillator texture between a softer and a more aggressive waveform character, making the sound feel more organic or more synthetic.";
    if (id == "TRANSIENT")
        return "Shapes the leading edge of the signal for more pick attack, tighter plucks, or a smoother, less abrupt response.";
    if (id.contains ("CHARACTER"))
        return "Changes the symmetry and character of the nonlinear response.";
    if (id.contains ("DRIFT") || id.contains ("JITTER") || id.contains ("WOBBLE"))
        return "Adds slow, musical movement that keeps the sound from feeling too static.";
    if (id.contains ("SCOOP") || id.contains ("TILT") || id.contains ("FOCUS"))
        return "Shifts the tonal balance toward a more sculpted midrange or a more open response.";
    if (id.contains ("CUTOFF") || id.contains ("FREQ") || id.contains ("FILTER"))
        return "Sets the frequency point where the filter starts shaping the spectrum.";
    if (id.contains ("RESONANCE") || id.contains ("Q"))
        return "Boosts the filter peak at the cutoff point for more bite, body, or sharper emphasis.";
    if (id.contains ("GLIDE") || id.contains ("PORTAMENTO"))
        return "Sets how smoothly notes move from one frequency to the next, which helps the sound feel more expressive and less abrupt.";
    if (id.contains ("GATE") || id.contains ("NOISE") && id.contains ("GATE"))
        return "Sets how strongly quiet or unwanted signal is reduced.";
    if (id.contains ("NOISE") || id.contains ("HISS") || id.contains ("CRACKLE"))
        return "Adds or removes texture, grit, or background noise for a more organic or lo-fi character.";
    if (id.contains ("PAN") || id.contains ("BALANCE"))
        return "Sets the position of the signal in the stereo field for a wider or more centered response.";
    if (id.contains ("RATIO") || id.contains ("THRESH") || id.contains ("KNEE"))
        return "Controls how aggressively the processor reacts above the threshold, which strongly affects the feel of compression or limiting.";
    if (id.contains ("ATTACK") || id.contains ("RELEASE") || id.contains ("DECAY"))
        return "Sets the speed of the response: tighter and faster for punch, slower for a smoother and more natural feel.";
    if (id.contains ("GAIN") || id.contains ("LEVEL") || id.contains ("OUTPUT"))
        return "Sets the overall strength of the signal after the effect stage; useful for balancing and keeping the output musical.";
    if (id.contains ("GATE"))
        return "Sets how strongly quiet or unwanted signal is reduced.";

    return displayName.isNotEmpty() ? displayName : parameterID;
}
}
