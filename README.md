# Beat MIDI Generator (VST3)

This repository contains a VST3 MIDI generator plugin built with the Steinberg VST3 SDK. The plugin outputs MIDI notes based on a Euclidean beat engine and exposes parameters for 8 beat lanes. A minimal VSTGUI editor is included, currently showing controls for Beat 1 only.

## Features
- MIDI-only generator (instrument with silent audio output for host compatibility)
- Euclidean rhythm engine (Bjorklund)
- 8 beat lanes, parameterized by Bars, Loop, Beats, Rotate, NoteIndex, Octave, Loud
- Parameter state saved/restored by the host
- Basic VSTGUI editor (Beat 1 controls)

## Requirements
- Steinberg VST3 SDK (set `VST3_SDK_ROOT`)
- CMake 3.20+
- Visual Studio 2022 (MSVC toolchain)

## Build and deploy the VST
From the repo root (cmd.exe):
```
set VST3_SDK_ROOT=C:\projects\ableplugs\vst3sdk
build.bat
```
This builds Release and copies `build\\VST3\\Release\\Beat.vst3` to
`C:\\ProgramData\\vstplugins\\Beat.vst3`.

For Debug (inline UI editor):
```
set VST3_SDK_ROOT=C:\projects\ableplugs\vst3sdk
build-debug.bat
```
This builds Debug and copies `build\\VST3\\Debug\\Beat.vst3` to
`C:\\ProgramData\\vstplugins\\Beat.vst3`.

Note: The SDK post-build step may try to create a symlink under
`%LOCALAPPDATA%\\Programs\\Common\\VST3`. If you do not have symlink
permissions, the build still produces a valid bundle; the link step can
be ignored.

## How to use the VST
1) Add "AblePlugs / Beat MIDI Generator" on a MIDI track in Live.
2) Create another MIDI track, set its input to the Beat track, and arm it.
3) Press Play in Live; the Beat plugin outputs MIDI to the armed track.
4) Use Beat Select to pick which beat lane you are editing, then adjust
   Bars/Loop/Beats/Rotate/Note/Octave/Loud sliders.
5) Click Reset if a saved state gets into a bad configuration.

If you need to edit the UI, build Debug, open the plugin UI, then right-click
to open the UIDescription Editor and save changes.

## Project Layout
- `vst3/src/BeatEngine.*` Euclidean beat engine
- `vst3/src/BeatProcessor.*` VST3 processor (MIDI output)
- `vst3/src/BeatController.*` VST3 controller and UI hookup
- `vst3/beat.uidesc` VSTGUI editor description
