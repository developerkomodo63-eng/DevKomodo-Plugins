# Release media checklist

The repository contains the editable quickstart source at
`docs/DEVKOMODO_QUICKSTART.md`. The final sales package should include seven
short audio examples and seven UI screenshots.

## Audio demos

Use the same dry DI performances for every comparison:

- Clean rhythm guitar, 20 seconds.
- Edge-of-breakup lead, 20 seconds.
- High-gain rhythm, 20 seconds.
- Bass groove, 20 seconds.
- Ambient chord progression, 20 seconds.
- Texture/synth phrase, 20 seconds.
- Vocal phrase for Vocal Shifter/Cleanup Pro, 20 seconds.

Render each example as 24-bit WAV at 48 kHz, with three labeled sections:
`DRY`, `DEVKOMODO`, and `BYPASS MATCH`. Keep the same input and output gain,
and do not master the examples. Use filenames such as
`01-overdrive-edge.wav` and `06-synth-texture.wav`.

## Screenshots

Run the signed release build at 100% UI scale and capture these categories:

- `docs/screenshots/drive.png`
- `docs/screenshots/dynamics.png`
- `docs/screenshots/modulation.png`
- `docs/screenshots/delay-reverb.png`
- `docs/screenshots/eq.png`
- `docs/screenshots/texture-synth.png`
- `docs/screenshots/utility.png`

Show the complete editor, one useful factory preset selected, and no host
branding or personal file paths. Capture AmpSim with its cabinet strip visible
and Convolution Reverb with its IR strip visible.

## PDF render

After screenshots exist, render the Markdown source with a PDF-capable Markdown
renderer, for example:

```text
pandoc docs/DEVKOMODO_QUICKSTART.md \
  --from gfm --toc --pdf-engine=xelatex \
  -o docs/DevKomodo-Quickstart.pdf
```

The PDF should be reviewed at 100% zoom and checked for clipped headings,
missing images, and readable screenshots before uploading it with the release.
