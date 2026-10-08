# BassFeel: low-end tuner (VST3 / AU)

Five macro knobs: **Weight, Punch, Tight, Grit, Width**. An Advanced drawer holds Crossover, Mono Lows and Output. Presets are chips along the top.

<img width="939" height="708" alt="safsdfasf" src="https://github.com/user-attachments/assets/a0f3d6b2-ad0a-4e02-8207-327860e04ce1" />

## Build

Requirements: CMake 3.22+, a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS), and internet access on the first configure (CMake downloads JUCE 8.0.4).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows with Visual Studio: `cmake -B build -G "Visual Studio 17 2022"` then `cmake --build build --config Release`.

The VST3 is copied to your system VST3 folder after the build. On Windows that is `C:\Program Files\Common Files\VST3`, which may need an admin terminal. Otherwise find it under `build/BassFeel_artefacts/Release/VST3`.

A Standalone app is also built, which is handy for quick testing without a DAW.

## Use in your DAWs

- **Ableton:** Preferences > Plug-ins > enable "Use VST3 Plug-in System Folders", then Rescan.
- **FL Studio:** Options > Manage plugins > Find installed plugins (rescan). Add it from the plugin database.
- On Mac, AU is built as well; Ableton on Mac can use either.

## How the DSP works

Input -> Linkwitz-Riley crossover (60-250 Hz, default 120)
- **Low band:** Weight (gain), Tight (expander on the decay tail), Mono Lows (side channel removed)
- **High band:** Width (mid/side), Grit (2x oversampled tanh saturation with slight asymmetry)
- Grit is fed the *full* signal and only the part above the crossover is mixed in, so a pure sub sine still gains audible harmonics.
- Punch: transient shaper (fast vs slow envelope) on the summed output.
- Latency from the oversampler is reported to the host, and the low and high paths are delayed to stay phase-aligned.

## Things to tune by ear

- `PluginProcessor.cpp`: grit `drive` range (`1 + g*10`) and loudness compensation.
- Tight detector times (`coef(60)`, `coef(400)`) and the `tg * 2.0f` exponent.
- Punch depth (`jlimit(0, 12)` dB).
- Preset values in `PluginEditor.cpp`.
