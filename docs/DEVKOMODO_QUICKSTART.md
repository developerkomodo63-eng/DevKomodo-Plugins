# DevKomodo Pedals

## Quickstart

DevKomodo is a guitar and bass effects suite. Insert a plugin on an audio
track, start with the `INIT` preset, match the input level, and use the output
`Level` control to match bypass loudness before comparing tones.

Tooltips are available on every control. Hover a knob, switch, or selector for
its purpose and the tradeoff it introduces.

## Drive and amp tone

**Overdrive, Distortion Guitar, Fuzz Guitar, Console Drive, Boost, Preamp,
AmpSim and Tone Sculptor** shape the instrument's harmonics and dynamics.

- `Drive`, `Gain`, or `Fuzz` controls how hard the nonlinear stage is pushed.
- `Tone`, `Bass`, `Mid`, `Treble`, `Presence`, `Body`, and `Air` shape frequency
  balance after the core character.
- `Mix` blends the processed sound with the original input.
- `Level` restores a useful output level after gain and compression.
- `HQ` enables 4x oversampling on AmpSim, Tone Sculptor, Multiband Drive,
  Distortion Guitar, Fuzz Guitar, and Overdrive. It is off by default and
  costs additional CPU.

Use `CLEAN` or low-drive settings for edge-of-breakup sounds, `PUNCH` for a
focused rhythm tone, and `LEAD` or `HEAVY` when sustain and upper harmonics are
wanted. In AmpSim, choose the cabinet before fine-tuning Presence and Level.

## Dynamics and tone shaping

**Compressor, Broadcast Compressor, Multiband Compressor, Noise Gate,
Transient Shaper, De-esser and Auto Swell** control level over time.

- `Threshold` sets where the processor starts working.
- `Ratio` sets the amount of compression above that point.
- `Attack` controls how quickly the transient is affected.
- `Release` controls how naturally the processor recovers.
- `Makeup` or `Level` restores output gain.
- `Mix` provides parallel compression when a little original transient is
  useful.

**EQ, Graphic EQ, Console EQ, Air Enhancer, Exciter and Phase Align** are for
balance and clarity. Cut unwanted buildup before adding broad boosts, then
match the result with `Level`.

## Modulation, delay and texture

**Chorus, Flanger, Phaser, Tremolo, Vibrato, Rotary Speaker, Doubler and Ring
Modulator** create movement. `Rate` or `Speed` sets motion, `Depth` sets its
intensity, and `Mix` determines how prominent it is.

**Delay, Tape Delay, Reverb, Spring Reverb and Convolution Reverb** provide
space. Start with a short `Time` or room, raise `Feedback` or decay gradually,
and keep `Mix` low enough that pick definition remains clear.

**Bitcrusher, Glitch Machine, Granulator, Vinyl Emulation, Tape Emulation,
Octaver, Synth Guitar, Synth Bass and Vocal Shifter** are character effects.
Use `Mix` for parallel texture and reduce `Level` if the processed signal
becomes louder than bypass.

## Presets and performance

The preset selector contains factory starting points and `MANUAL`. Changing a
control switches the workflow back to manual editing. Presets are stored with
the host project through the plugin state.

For live use, keep HQ off unless the extra clarity is worth the CPU budget.
For rendered or final tracks, enable HQ on high-drive settings and re-check
latency and level in the host.

## Screenshots for the release PDF

Export one screenshot per category from the final release build and place them
under `docs/screenshots/` using these names:

- `drive.png`
- `dynamics.png`
- `modulation.png`
- `delay-reverb.png`
- `eq.png`
- `texture-synth.png`
- `utility.png`

This Markdown source is intentionally kept editable so the PDF can be rendered
from the same release screenshots and version number.
