# DevKomodo Ultimate

## Release hardening

- Windows x64 VST3 only, with JUCE 8.0.15 pinned in CMake.
- One canonical GitHub Actions workflow for the full VST3 matrix.
- Removed global IPO duplication; LTO remains controlled by JUCE target flags.
- JUCE examples/extras are disabled for leaner CI configuration.

## DSP fixes

- Guitar/Bass fused profiles remain in the same VST3 targets.
- Distortion, Fuzz and Saturator families are intentionally differentiated.
- Reverb Bass width mapping is applied after type mapping, preventing Plate from undoing the mono-compatible bass profile.
- Shimmer processing is skipped when disabled to reduce CPU.
- Convolution Reverb stays transparent until an impulse response is actually active, avoiding a false wet-level attenuation on a fresh instance.

## UI fixes

- Universal editor header is responsive at the minimum window size and no longer overlaps controls.
- Generic status strip no longer displays a fabricated fixed input level.
- Slider value suffixes use the parameter's declared label.
- Instrument selector has explicit branded colours.
- Synth editor sliders return to their real parameter defaults on double-click instead of zero.

## Validation performed

- All C++/header files pass structural brace/parenthesis/bracket checks.
- All local quoted includes resolve.
- Plugin target names and VST3 product codes are unique.
- The project contains 51 CMake plugin targets and one canonical CI workflow.
- Full JUCE compilation could not be executed in this environment because external GitHub access is unavailable; the pinned JUCE source is still fetched by the existing Windows GitHub Actions workflow.
