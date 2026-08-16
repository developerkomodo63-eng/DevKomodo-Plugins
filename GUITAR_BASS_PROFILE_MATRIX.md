# Guitar/Bass profile matrix

All plugins in this matrix already expose the `INSTRUMENT` Guitar/Bass selector.
The DSP now treats the two modes as intentional instrument profiles rather than
a cosmetic switch.

| Plugin | Guitar profile | Bass profile |
|---|---|---|
| Envelope Filter | Higher sweep range / articulation | Lower tracking range, wider low-note envelope |
| Distortion | More upper-mid bite and scoop | Low-end preserved, less scoop |
| Fuzz | More harmonic aggression | Reduced fuzz and stronger low-end preservation |
| Overdrive | Brighter/upper-mid response | Fuller, controlled low end |
| Octaver | Useful octave-up content | Stronger sub/octave-down, almost no octave-up |
| AmpSim | Guitar cab family | Bass cab family automatically follows mode |
| Chorus | Stereo movement/width | L/R modulation phase offset disabled for mono-compatible bass |
| Phaser | Wider/lower sweep | Higher sweep, lower feedback/mix to protect fundamentals |
| Reverb | Full stereo ambience | Narrower wet field, stronger low-end wet filtering, reduced shimmer |

The changes are intentionally lightweight and do not add a new public API or
change CMake targets.
