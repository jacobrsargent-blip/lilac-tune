# Lilac Tune

One knob auto-tune. Light purple. That's it.

- **Live / studio** (pill, top left; tap to cycle auto → live → studio):
  - *auto* (default): switches to **live** (~6.5 ms) while your DAW is recording, and to **studio**
    (~23 ms look-ahead, cleaner tuning) on playback and bounces.
  - Your DAW records the dry vocal, so when you play it back it's re-tuned in studio quality.
    What you heard while recording is just the preview.
  - If your DAW doesn't tell plugins it's recording, set it to *live* while tracking and back to auto after.
- **The knob** (labelled "tune"): from barely there (gentle 20% pull, slow 300 ms glide that leaves vibrato alone)
  through *natural / polished / tuned / hard tune / t-pain* to **x50**: instant snapping,
  vibrato flattened, a buzzier grain and an exaggerated "flip" overshoot on every note change.
- **Key detection**: sing or play the vocal for a few seconds. The likely keys light up in the grid
  (relative major/minor often glow together, since they share the same notes). The outlined key is the
  best guess and is used automatically. Tap any key to lock it, tap it again to go back to auto.
  "re-detect" clears what it has heard.

## Build (Mac or Windows)

You need **CMake 3.22+** and a C++ compiler (Xcode on Mac, Visual Studio 2022 on Windows).
JUCE is downloaded automatically the first time.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The plugin is copied to your system plugin folder after building
(VST3: `~/Library/Audio/Plug-Ins/VST3` on Mac, `C:\Program Files\Common Files\VST3` on Windows;
AU on Mac only). Rescan plugins in your DAW. There's also a Standalone app in `build/` for quick testing.

> Windows: run the build from an Administrator terminal, or the copy into Program Files will fail.
> You can also copy `build/LilacTune_artefacts/Release/VST3/Lilac Tune.vst3` there yourself.

## How it works

`Source/dsp/` is plain C++ with no JUCE, so you can test it on its own:

| File | What it does |
|---|---|
| `PitchDetector.h` | YIN pitch tracking (70–1100 Hz) |
| `KeyDetector.h` | Pitch-class histogram matched against Krumhansl-Schmuckler key profiles |
| `PitchShifter.h` | Pitch-synchronous two-head shifter, grains locked to the voice period |
| `TuneEngine.h` | Maps the knob onto strength/speed/character and snaps to the key |

Latency is reported to the DAW for each mode, so playback stays in sync. Some DAWs only pick up a
latency change when the transport stops, which is when auto mode switches anyway.
Also set a small audio buffer (64–128 samples) while recording; it matters as much as the plugin.

## Ideas for v2
- Formant preservation (stops the "chipmunk" sound on big shifts)
- Custom scale (tap notes on/off)
- Resizable window
