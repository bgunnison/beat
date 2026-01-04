# Beat MIDI Generator (VST3)

This repository contains a VST3 MIDI generator plugin. The plugin outputs MIDI notes based on a Euclidean beat engine and has settings for 8 beat lanes. 

## Features
- MIDI-only generator 
- Euclidean rhythm engine
- 8 beat lanes, settings for Bars, Loop, Beats, Rotate, NoteIndex, Octave, Loudness
- Parameter state saved/restored by the host

## Requirements
- Steinberg VST3 SDK (set `VST3_SDK_ROOT`)
- CMake 3.20+
- Visual Studio 2022 (MSVC toolchain)

## Build and deploy the VST
From the repo root (cmd.exe):
```
set VST3_SDK_ROOT=yourpath\vst3sdk
build.bat
```
This builds Release `build\\VST3\\Release\\Beat.vst3` 

For Debug (inline UI editor):
```
build-debug.bat
```
This builds Debug `build\\VST3\\Debug\\Beat.vst3` 

Note: The SDK post-build step may try to create a symlink under
`%LOCALAPPDATA%\\Programs\\Common\\VST3`. If you do not have symlink
permissions, the build still produces a valid bundle; the link step can
be ignored.

## How to use the VST
1) Copy the beat.vst3 to your plugin folder.
2) Add "AblePlugs / Beat MIDI Generator" on a MIDI track in Live.
3) Create another MIDI track, set its input to the Beat track, and arm it.
4) Press Play in Live; the Beat plugin outputs MIDI to the armed track.
5) Use Beat Select to pick which beat lane you are editing, then adjust
   Bars/Loop/Beats/Rotate/Note/Octave/Loud sliders.
6) Click Reset if a saved state gets into a bad configuration.

