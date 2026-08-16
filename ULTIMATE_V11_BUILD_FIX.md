# DevKomodo Ultimate v11 — Build Fix

## GitHub Actions failure fixed

The Windows build stopped in `Pedals/VocalShifter/Source/DevKomodoUI.h` because the universal preset code used the legacy `AudioProcessor::getParameter(int)` API with string IDs.

### Fix
- Added a JUCE-8-compatible `findParameterById()` helper that resolves parameters through `AudioProcessor::getParameters()` and `RangedAudioParameter::paramID`.
- Replaced all `processorRef.getParameter("...")` calls in the shared editor, including `VOICE` and `STYLE`.
- Replaced the shadowing declaration in `setPresetValue()` that caused MSVC C3536 (`parameter` used before initialization).
- Applied the fix to all 51 plugin copies of `DevKomodoUI.h`, not only VocalShifter, so the same failure cannot simply move to the next target.

No DSP or UI feature was removed by this build fix.
