# DevKomodo Trial Policy

The review/demo build is intended for evaluation before purchase.

- Trial duration: 15 minutes of cumulative audio playback per process.
- The timer counts host audio blocks, not editor-open time.
- All plugins in the same process share the timer, so opening another plugin
  does not reset the trial.
- The UI is marked `DEMO` so review copies cannot be confused with release
  binaries.
- After the limit, processing becomes silent until the host process restarts.
- Factory presets, parameter automation, state recall, VST3 and AU formats are
  available during the trial.

Configure the build with:

```text
cmake -S . -B build-demo -DDEVKOMODO_DEMO_BUILD=ON
cmake --build build-demo --config Release
```

This is a local review build policy, not a complete online licensing system.
For paid distribution, use a signed licensing service or license manager with
machine activation, revocation, offline grace periods, and a privacy policy.