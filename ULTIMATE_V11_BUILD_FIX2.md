# DevKomodo Plugins — ULTIMATE v11 Build Fix 2

## GitHub Actions / JUCE 8 compatibility fixes

- Fixed `juce::Slider` text callbacks to use the JUCE 8 public members `textFromValueFunction` and `valueFromTextFunction`.
- Removed the invalid `RangedAudioParameter::getNumDecimalPlacesToDisplay()` call.
- Added lightweight precision inference from `NormalisableRange::interval`.
- Fixed the AmpSim choice conversion to pass a float explicitly.
- Removed local-variable shadowing in preset assignment for clearer MSVC diagnostics.
- Applied the UI fix to all 51 plugin `DevKomodoUI.h` files, not just VocalShifter/VinylEmulation.
- No oversampling added.
- No DSP-heavy UI components added.

## Static verification

- 51 `DevKomodoUI.h` files patched.
- 0 remaining references to the invalid Slider setter APIs.
- 0 remaining references to `RangedAudioParameter::getNumDecimalPlacesToDisplay()`.
- 0 direct `processorRef.getParameter(...)` calls.
- APVTS parameter lookup remains ID-based through `apvts.getParameter(...)`.
