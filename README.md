# DevKomodo Plugins — v1.0

A suite of 49 guitar/bass effects plugins (VST3), built with JUCE.

## Installing

Copy the contents of the Windows VST3 build artifact into your system VST3 folder:

```
C:\Program Files\Common Files\VST3\
```

Then rescan plugins in your DAW.

Windows releases also include an Inno Setup installer with optional plugin
categories. The installer places selected plugins in the standard VST3 folder
and keeps the raw VST3 artifact available for manual installation.

## Release signing and notarization

The release workflow signs Windows VST3 binaries when these GitHub Actions
secrets are configured:

- `WINDOWS_CODESIGN_CERTIFICATE_BASE64`: base64-encoded `.pfx` certificate.
- `WINDOWS_CODESIGN_CERTIFICATE_PASSWORD`: password for that certificate.
- `WINDOWS_CODESIGN_TIMESTAMP_URL`: optional RFC 3161 timestamp service URL.

macOS releases additionally require these repository secrets:

- `APPLE_CERTIFICATE_BASE64`: base64-encoded Developer ID certificate export.
- `APPLE_CERTIFICATE_PASSWORD`: password for the certificate export.
- `APPLE_DEVELOPER_ID`: Developer ID Application identity.
- `APPLE_TEAM_ID`: Apple Developer team identifier.
- `APPLE_KEYCHAIN_PASSWORD`: temporary CI keychain password.
- `APPLE_NOTARY_KEY_ID`, `APPLE_NOTARY_ISSUER_ID`, `APPLE_NOTARY_KEY_BASE64`:
	App Store Connect API key credentials for `notarytool`.

Signing is conditional: ordinary pull requests can still build without release
credentials, while tagged or manually dispatched releases use them when set.

## Plugin list (49)

**Drive / Distortion** — Boost, Clipper, Console Drive, Distortion, Fuzz, Overdrive, Preamp, CleanUp Pro, Tone Sculptor, AmpSim, Multiband Drive

**Dynamics** — Compressor (with built-in Limiter mode), Broadcast Compressor, Multiband Compressor, Noise Gate, Transient Shaper, De-esser, Auto Swell

**Modulation** — Chorus, Flanger, Phaser, Tremolo, Vibrato, Rotary Speaker, Doubler, Ring Modulator, Envelope Filter

**Delay / Reverb** — Delay, Tape Delay, Reverb, Spring Reverb, Convolution Reverb

**EQ / Tone shaping** — EQ 6-Band, Graphic EQ, Air Enhancer, Exciter, Phase Align

**Texture / Synth** — Bitcrusher, Glitch Machine, Granulator, Vinyl Emulation, Tape Emulation, Octaver, Synth Guitar, Synth Bass, DevKomodo Vocal Shifter

**Utility** — Utility Gain, Mono Maker, Drum Enhancer

## Building from source

Requires CMake 3.22+ and a C++ toolchain. JUCE is fetched automatically via CMake `FetchContent` at configure time (see `CMakeLists.txt`).

```
cmake -B build
cmake --build build --config Release
```

### Demo build

For review copies, configure with `-DDEVKOMODO_DEMO_BUILD=ON`. The demo marks
the UI as DEMO and stops producing audio after 15 minutes of cumulative host
playback. The commercial build leaves this option OFF.

> **Before distributing a commercial build:** this project fetches JUCE
> directly from the official JUCE repository. Confirm you're covered by a
> JUCE license tier appropriate for commercial/paid distribution (JUCE's
> free tier has revenue limits and a mandatory splash screen requirement —
> see https://juce.com/get-juce for current terms). This is a licensing
> decision for the project owner, not something this build configures for
> you automatically.

## License

See `LICENSE`. All plugin DSP/UI source in this repository is © DevKomodo.
