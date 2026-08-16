# Guitar/Bass product differentiation

## SynthGuitar
- Tighter low-end behavior so the guitar synth does not become phasey when the
  input contains strong low harmonics.
- Conservative stereo character on upper content.
- The fundamental remains phase-coherent/centered.

## SynthBass
- Stronger mono compatibility in the low register.
- No artificial phase-offset doubling of the fundamental.
- More conservative widening because bass cancellation is much more audible
  on club/PA/sub systems.

These changes are deliberately lightweight and do not alter the public plugin
API or the existing CMake target structure.
