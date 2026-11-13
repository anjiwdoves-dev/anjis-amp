# Anji's Amp

A guitar amp simulator plugin (VST3 / AU / Standalone) with a clean, minimal UI.

- **9 amp models**: Glass (American clean), Twinkle (small-combo emo clean), Chime (British class-A), Basement (big British crunch), Plexi, 800, Brown, Modern (high gain), Jazz (solid-state clean)
- **5 cabinets + IR loader** for your own impulse responses, with auto level matching
- **Pedals**: noise gate, compressor, drive (Green screamer, Gold transparent, Treble booster, Fuzz)
- **Effects**: tremolo, stereo chorus/vibrato, analog-style delay, stereo reverb
- **32 level-matched factory presets** across Midwest Emo, Classic Rock, Clean and Heavy
- Save and load your own presets (`.anjipreset`, stored in `Documents/Anjis Amp/Presets`)
- 2× oversampled amp stage to cut down on harsh aliasing

## Get the plugin (easiest: let GitHub build it)

1. Create a new GitHub repo and upload this whole folder (including the hidden `.github` folder).
2. Open the repo's **Actions** tab. The "Build Anji's Amp" workflow runs on its own (or click **Run workflow**).
3. When it finishes (about 10–15 minutes), download **AnjisAmp-macOS** or **AnjisAmp-Windows** from the run's Artifacts section.

## Install

**macOS**: unzip, then copy
- `Anjis Amp.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- `Anjis Amp.component` → `~/Library/Audio/Plug-Ins/Components/` (Logic, GarageBand)

The build isn't code-signed, so macOS may block it. Run this once in Terminal:
```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Anjis Amp.vst3" ~/Library/Audio/Plug-Ins/Components/"Anjis Amp.component"
```

**Windows**: copy `Anjis Amp.vst3` into `C:\Program Files\Common Files\VST3\`.

Then rescan plugins in your DAW. `Anjis Amp.app` / `.exe` is a standalone version for practice without a DAW.

## Build it yourself

You need CMake 3.22+ and Xcode (Mac) or Visual Studio 2022 with C++ (Windows). JUCE downloads automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
The plugins end up in `build/AnjisAmp_artefacts/Release/`.

## Tips

- Put the plugin on a **mono** track recording your guitar DI (dry input), and set your interface's input so peaks land around −12 to −6 dBFS. Use the **In** knob if a preset feels too hot or too tame.
- Presets are level-matched, so switching between them won't jump in volume. Clean presets sit a little lower so their pick attack doesn't clip.
- Double-click any knob to reset it.

## Project layout

| Path | What it is |
|---|---|
| `Source/AnjiDSP.h` | The whole sound engine (amps, cabs, pedals, FX), framework-free |
| `Source/ParamTable.h` | Parameter IDs, ranges, defaults |
| `presets.json` | Factory presets: edit these, then run `python3 tools/gen_presets.py` |
| `Source/Plugin*.{h,cpp}` | JUCE wrapper and UI |
| `web/` | Browser version using the same engine (`engine.js` is a port of `AnjiDSP.h`) |
| `tools/` | Test harness, level calibration, C++/JS parity check |

To add or tweak an amp, edit its row in `kAmps` in `AnjiDSP.h`, then rerun `tools/gen_presets.py` so the web version picks it up.
