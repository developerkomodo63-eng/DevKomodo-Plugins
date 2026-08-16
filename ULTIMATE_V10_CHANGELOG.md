# DevKomodo Plugins — Ultimate v10 / Fused Release

## Distribution
- Fused the complete Guitar/Bass and Synth plugin sets into one repository root.
- 51 buildable VST3 plugin targets are present under `Pedals/`.
- Removed stale, unused `DevKomodoLookAndFeel.h` declarations from the three legacy folders that contained them.
- Preserved the Windows x64 / VST3-only CMake and GitHub Actions structure.

## UI / Controls
- Reworked the universal DevKomodo knob look-and-feel across all 51 plugins.
- Knobs now have clearer arc/pointer feedback, visible parameter names, safer drag sensitivity and reliable double-click reset.
- Normalized controls such as Mix/Depth/Width/Amount now display percentages instead of raw 0–1 values.
- Responsive layouts were retained so the header and controls do not overlap at small sizes.
- Synth Guitar and Synth Bass now show parameter names directly on the knobs and have a dedicated WAVEFORM label.
- Fixed Synth preset waveform loading: presets now write the actual waveform choice instead of a doubly-normalized value.

## AmpSim
- Reworked Gain, Bass, Mid, Treble, Presence and Level to the musical 0–10 control convention.
- Internal DSP maps those 0–10 controls to the original dB/drive ranges, preserving the intended sonic scale.
- Added AmpSim-specific factory presets: Clean, Crunch, Lead and Modern.

## Premium plugins
### CleanUp Pro
- Replaced the placeholder processing with a lightweight three-stage cleanup engine:
  - smoothed gate detector;
  - frequency-aware high-band de-essing;
  - transient enhancement.
- No FFTs or per-block allocations in the audio loop.
- Added dedicated Vocal / Guitar / Punch / Surgical factory presets.

### Tone Sculptor
- Drive is now a 0–10 control.
- Added Body and Air controls.
- Tone is now a real spectral tilt rather than a mathematical amplitude split.
- Added stateful low/high shaping with musical broad-band tone controls.
- Added Warm / Bright / Body / Modern factory presets.

### Drive family
- Overdrive, Distortion, Fuzz, Preamp, Console Drive and Saturator drive controls now use a consistent 0–10 workflow while mapping internally to their useful DSP ranges.
- Guitar/Bass-specific processing remains intact.

## Validation
- All C/C++ structural brace/parenthesis/bracket checks pass.
- All local quoted includes resolve.
- CMake has 51 plugin targets and 51 plugin directories with CMakeLists.txt; no target is missing.
- Plugin codes remain unique.

## Build note
The source tree is prepared for the existing GitHub Windows build. A complete JUCE/MSVC compile cannot be executed in this environment because the JUCE source is fetched by CMake from GitHub and is not installed locally here.
