# Beat (VST3 MIDI Generator)

Beat is a VST3 MIDI generator/instrument. It produces MIDI note patterns from eight Euclidean rhythm lanes and uses a shared edit section for the currently selected lane.

## Features
- 8 beat lanes
- Per-lane parameters: `Bars`, `Loop`, `Beats`, `Rotate`, `Note`, `Octave`, `Loud`
- Lane select buttons `1` through `8`
- Per-lane `M` and `S` controls plus global `Mute All`, `Global Solo`, and `Reset`
- Lane activity feedback
- Host parameter state save/restore
- MIDI output with a silent stereo audio output for hosts that expect an instrument bus

## Requirements
- Steinberg VST3 SDK via `VST3_SDK_ROOT`
- CMake 3.20+
- Visual Studio 2022 / MSVC

## Build
From the repo root in `cmd.exe`:

```bat
set VST3_SDK_ROOT=C:\path\to\vst3sdk
build.bat
```

`build.bat` configures `vst3` into `build` and builds both Debug and Release.
The build outputs are created under `build\VST3\Debug` and `build\VST3\Release`.

Note: the Steinberg SDK post-build step may try to create a symlink under `%LOCALAPPDATA%\Programs\Common\VST3`. If symlink creation fails, the local bundle output is still usable.

## Deploy
Use:

```bat
deploy.bat
```

This installs:
- `C:\ProgramData\vstplugins\DebugBeat.vst3`
- `C:\ProgramData\vstplugins\Beat.vst3`

`deploy.bat` also copies the current UI resources from `vst3\beat.uidesc` and `vst3\logo_strip.png` into both installed bundles, so UI layout edits are picked up on deploy.

## Use In Ableton Live
1. Add `VirtualRobot / Beat` to a MIDI track.
2. Create another MIDI track with your instrument on it.
3. Set the instrument track's MIDI input to the Beat track and arm or monitor that track.
4. Start playback in Live.
5. Use lane buttons `1` to `8` to choose which beat lane you are editing.
6. Adjust `Bars`, `Loop`, `Beats`, `Rotate`, `Note`, `Octave`, and `Loud` for the selected lane.
7. Use `M`, `S`, `Mute All`, `Global Solo`, and `Reset` as needed.

If you install the debug build, the plugin name is `Debug Beat`.
