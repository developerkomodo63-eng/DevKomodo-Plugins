# DevKomodo Plugins — Ultimate v11

## Design goals

- Windows x64 + VST3 only.
- Low CPU and low latency are priorities.
- No oversampling in the shipped nonlinear processors.
- Consistent 0–10 UI for amp/pedal intensity controls where a 0–10 workflow makes musical sense.
- Visually coherent UI without expensive real-time graphics.

## Gain family separation

- **Boost:** clean gain stage, 0–10 control mapped internally to 0–24 dB.
- **Preamp:** two-stage low-gain asymmetric tube-style shaping plus tone stack.
- **Overdrive:** asymmetric soft clipping and harmonic coloration.
- **Distortion:** steeper hard-knee clipping with bias and mid-scoop behavior.
- **Fuzz:** waveform folding + diode-style behavior + bias, distinct from distortion.
- **Saturator:** four genuinely different transfer families: Tube, Tape, Diode, Hard.
- **Console Drive:** subtle level-dependent compression and real spectral tilt.
- **Tape Emulation:** dedicated tape-style delay/warble/saturation path.
- **AmpSim:** two native-rate nonlinear amp stages followed by tone/cab filtering.
- **Tone Sculptor:** Tube/Tape/Console/Edge styles plus body/air/tone shaping.

## CPU / DSP

- Removed oversampling stages from Overdrive, Distortion, Fuzz, AmpSim and Saturator.
- Removed the associated latency and oversampling buffers.
- Kept nonlinear processing allocation-free inside `processBlock`.
- Continued using lightweight IIR/state-variable filters instead of FFT processing where possible.
- Kept premium processing native-rate and deterministic.

## Premium plugins

### CleanUp Pro

Added:
- Mix control.
- Output trim.
- Existing gate/de-esser/transient stages remain FFT-free.
- Presets now explicitly restore neutral mix/output values.

### Tone Sculptor

Added:
- Style selector: Tube / Tape / Console / Edge.
- Distinct nonlinear behavior per style.
- Drive compensation to keep the control more musical.
- Existing Body / Air / Tone architecture retained.
- Premium preset styles map to the four transfer families.

## UI

- Improved knob drag precision from the previous global sensitivity.
- Dark value boxes with stronger contrast and readable text.
- Automatic display units for Hz, ms, dB and 0–10 controls.
- Amp-style controls visibly read as `value / 10` rather than arbitrary ±dB ranges.
- Added lightweight control cards and a Premium badge for major premium processors.
- Synth editor value boxes are larger and clearer; rotary control sensitivity is consistent.
- Double-click returns parameters to their real parameter defaults.
- No CPU-heavy analyzers or animated graphics were added.

## Validation

- 51 plugin targets remain present.
- Structural delimiter scan: 0 problems across C++, headers and CMake files.
- No remaining `Oversampling`/`oversampler` references in the source tree.
- Windows-only VST3 CI remains the release build path.

> Full MSVC/JUCE compilation must be performed on the Windows GitHub Actions runner because the project intentionally rejects non-Windows configuration and the local environment does not provide the Windows JUCE/MSVC toolchain.
