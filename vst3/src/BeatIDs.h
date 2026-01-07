#pragma once

#include "pluginterfaces/base/fplatform.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "BeatEngine.h"

#ifdef BEAT_DEBUG_UIDS
// {6F1A8F9B-0B7F-4D6A-8F63-3D6D2D0B6C21}
static const Steinberg::FUID kBeatProcessorUID (0x6F1A8F9B, 0x0B7F4D6A, 0x8F633D6D, 0x2D0B6C21);
// {B5C2E1A8-3C16-4B78-9B62-7F1E4A02D9F5}
static const Steinberg::FUID kBeatControllerUID (0xB5C2E1A8, 0x3C164B78, 0x9B627F1E, 0x4A02D9F5);
#else
// {C6D2C49A-3E6B-47A9-AE0E-8F9F8C75F4A2}
static const Steinberg::FUID kBeatProcessorUID (0xC6D2C49A, 0x3E6B47A9, 0xAE0E8F9F, 0x8C75F4A2);
// {E4C71F7D-8B94-4DB2-B41B-6C5FBB6E2C11}
static const Steinberg::FUID kBeatControllerUID (0xE4C71F7D, 0x8B944DB2, 0xB41B6C5F, 0xBB6E2C11);
#endif

#ifdef BEAT_DEBUG_NAME
constexpr Steinberg::FIDString kBeatVst3Name = "Debug Beat";
#else
constexpr Steinberg::FIDString kBeatVst3Name = "Beat";
#endif
constexpr Steinberg::FIDString kBeatVst3Vendor = "AblePlugs";
constexpr Steinberg::FIDString kBeatVst3Version = "0.1.0 (" __DATE__ " " __TIME__ ")";
constexpr const char kBeatVst3Build[] = __DATE__ " " __TIME__;
constexpr Steinberg::FIDString kBeatVst3Url = "https://ableplugs.local/beat";
constexpr Steinberg::FIDString kBeatVst3Email = "support@ableplugs.local";

enum ParamIDs : Steinberg::Vst::ParamID {
    kParamEffectEnabled = 0,
    kParamBeatSelect,
    kParamReset,
    kParamBaseBeatParams // start of per-beat params, layout: beat * kPerBeatParams + param
};

constexpr int kPerBeatParams = 7; // Bars, Loop, Beats, Rotate, NoteIndex, Octave, Loud
constexpr int kActiveParamBase = kParamBaseBeatParams + beatvst::kMaxBeats * kPerBeatParams;
constexpr int kLaneMuteBase = kActiveParamBase + kPerBeatParams;
constexpr int kLaneSoloBase = kLaneMuteBase + beatvst::kMaxBeats;
constexpr int kLaneActivityBase = kLaneSoloBase + beatvst::kMaxBeats;

inline Steinberg::Vst::ParamID beatParamId(int beatIndex, int paramSlot) {
    return static_cast<Steinberg::Vst::ParamID>(kParamBaseBeatParams + beatIndex * kPerBeatParams + paramSlot);
}

enum BeatParamSlot {
    kSlotBars = 0,
    kSlotLoop,
    kSlotBeats,
    kSlotRotate,
    kSlotNoteIndex,
    kSlotOctave,
    kSlotLoud
};

enum ActiveParamSlot {
    kActiveBars = 0,
    kActiveLoop,
    kActiveBeats,
    kActiveRotate,
    kActiveNoteIndex,
    kActiveOctave,
    kActiveLoud
};

inline Steinberg::Vst::ParamID activeParamId(int slot) {
    return static_cast<Steinberg::Vst::ParamID>(kActiveParamBase + slot);
}

inline Steinberg::Vst::ParamID laneMuteParamId(int beatIndex) {
    return static_cast<Steinberg::Vst::ParamID>(kLaneMuteBase + beatIndex);
}

inline Steinberg::Vst::ParamID laneSoloParamId(int beatIndex) {
    return static_cast<Steinberg::Vst::ParamID>(kLaneSoloBase + beatIndex);
}

inline Steinberg::Vst::ParamID laneActivityParamId(int beatIndex) {
    return static_cast<Steinberg::Vst::ParamID>(kLaneActivityBase + beatIndex);
}
