#include "BeatController.h"

#include "BeatEngine.h"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uidescription.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

namespace beatvst {
using namespace Steinberg;
using namespace Steinberg::Vst;

tresult PLUGIN_API BeatController::initialize(FUnknown* context) {
    tresult res = EditControllerEx1::initialize(context);
    if (res != kResultOk) return res;

    // Effect on/off
    RangeParameter* effectEnabled = new RangeParameter(STR16("Mute"), ParamIDs::kParamEffectEnabled, nullptr, 0.0, 1.0, 0.0);
    parameters.addParameter(effectEnabled);

    RangeParameter* beatSelect = new RangeParameter(STR16("Beat Select"), ParamIDs::kParamBeatSelect, nullptr, 1.0, static_cast<ParamValue>(kMaxBeats), 1.0);
    beatSelect->setPrecision(0);
    parameters.addParameter(beatSelect);

    RangeParameter* resetParam = new RangeParameter(STR16("Reset"), ParamIDs::kParamReset, nullptr, 0.0, 1.0, 0.0);
    resetParam->setPrecision(0);
    parameters.addParameter(resetParam);

    auto addActiveParam = [&](const std::string& label, ParamID id, double min, double max, double def, int precision) {
        String128 title{};
        UString(title, str16BufferSize(String128)).fromAscii(label.c_str());
        auto* p = new RangeParameter(title, id, nullptr, min, max, def);
        p->setPrecision(precision);
        p->getInfo().flags |= ParameterInfo::kIsHidden;
        parameters.addParameter(p);
    };

    addActiveParam("Bars", activeParamId(ActiveParamSlot::kActiveBars), 1, kMaxLoopLength, 4, 0);
    addActiveParam("Loop", activeParamId(ActiveParamSlot::kActiveLoop), 1, kMaxLoopLength, 16, 0);
    addActiveParam("Beats", activeParamId(ActiveParamSlot::kActiveBeats), 0, kMaxLoopLength, 4, 0);
    addActiveParam("Rotate", activeParamId(ActiveParamSlot::kActiveRotate), 0, kMaxLoopLength, 0, 0);
    addActiveParam("NoteIndex", activeParamId(ActiveParamSlot::kActiveNoteIndex), 0, 11, 0, 0);
    addActiveParam("Octave", activeParamId(ActiveParamSlot::kActiveOctave), kMinOctave, kMaxOctave, 2, 0);
    addActiveParam("Loud", activeParamId(ActiveParamSlot::kActiveLoud), 0, 127, 0, 0);

    auto addBeatParam = [&](int beat, const std::string& label, ParamID id, double min, double max, double def, int precision, UnitID unitId) {
        String128 title{};
        UString(title, str16BufferSize(String128)).fromAscii(label.c_str());
        auto* p = new RangeParameter(title, id, nullptr, min, max, def);
        p->setPrecision(precision);
        p->setUnitID(unitId);
        parameters.addParameter(p);
    };

    for (int b = 0; b < kMaxBeats; ++b) {
        addBeatParam(b, "Bars", beatParamId(b, BeatParamSlot::kSlotBars), 1, kMaxLoopLength, 4, 0, kRootUnitId);
        addBeatParam(b, "Loop", beatParamId(b, BeatParamSlot::kSlotLoop), 1, kMaxLoopLength, 16, 0, kRootUnitId);
        addBeatParam(b, "Beats", beatParamId(b, BeatParamSlot::kSlotBeats), 0, kMaxLoopLength, 4, 0, kRootUnitId);
        addBeatParam(b, "Rotate", beatParamId(b, BeatParamSlot::kSlotRotate), 0, kMaxLoopLength, 0, 0, kRootUnitId);
        addBeatParam(b, "NoteIndex", beatParamId(b, BeatParamSlot::kSlotNoteIndex), 0, 11, b % 12, 0, kRootUnitId);
        addBeatParam(b, "Octave", beatParamId(b, BeatParamSlot::kSlotOctave), kMinOctave, kMaxOctave, 2, 0, kRootUnitId);
        addBeatParam(b, "Loud", beatParamId(b, BeatParamSlot::kSlotLoud), 0, 127, 0, 0, kRootUnitId);

        auto* laneMute = new RangeParameter(STR16("Lane Mute"), laneMuteParamId(b), nullptr, 0.0, 1.0, 0.0);
        laneMute->setPrecision(0);
        parameters.addParameter(laneMute);

        auto* laneSolo = new RangeParameter(STR16("Lane Solo"), laneSoloParamId(b), nullptr, 0.0, 1.0, 0.0);
        laneSolo->setPrecision(0);
        parameters.addParameter(laneSolo);

        auto* laneActivity = new RangeParameter(STR16("Lane Activity"), laneActivityParamId(b), nullptr, 0.0, 1.0, 0.0);
        laneActivity->setPrecision(0);
        laneActivity->getInfo().flags = ParameterInfo::kIsReadOnly;
        parameters.addParameter(laneActivity);
    }

    buildParamOrder();
    return kResultOk;
}

IPlugView* PLUGIN_API BeatController::createView(FIDString name) {
    if (FIDStringsEqual(name, ViewType::kEditor)) {
        // Let VST3Editor load the view; if missing, host will fall back.
        return new VSTGUI::VST3Editor(this, "view", "beat.uidesc");
    }
    return EditControllerEx1::createView(name);
}

tresult PLUGIN_API BeatController::getState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        ParamValue v = getParamNormalized(pid);
        if (!streamer.writeDouble(v)) return kResultFalse;
    }
    return kResultOk;
}

tresult PLUGIN_API BeatController::setState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double v = 0.0;
        if (!streamer.readDouble(v)) return kResultFalse;
        setParamNormalized(pid, v);
    }
    syncActiveParams();
    return kResultOk;
}

tresult PLUGIN_API BeatController::setParamNormalized(ParamID pid, ParamValue value) {
    tresult res = EditControllerEx1::setParamNormalized(pid, value);
    if (pid == ParamIDs::kParamBeatSelect) {
        syncActiveParams();
        return res;
    }
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kPerBeatParams) {
        if (!syncingActive_) {
            const int slot = static_cast<int>(pid - kActiveParamBase);
            const int beatIndex = selectedBeatIndex();
            const ParamID perBeatId = beatParamId(beatIndex, slot);
            syncingActive_ = true;
            EditControllerEx1::setParamNormalized(perBeatId, value);
            if (componentHandler) {
                componentHandler->beginEdit(perBeatId);
                componentHandler->performEdit(perBeatId, value);
                componentHandler->endEdit(perBeatId);
            }
            syncingActive_ = false;
        }
        return res;
    }
    if (pid == ParamIDs::kParamReset && value > 0.5) {
        resetAllParams();
        beginEdit(ParamIDs::kParamReset);
        performEdit(ParamIDs::kParamReset, 0.0);
        endEdit(ParamIDs::kParamReset);
        EditControllerEx1::setParamNormalized(ParamIDs::kParamReset, 0.0);
    }
    return res;
}

bool BeatController::isNoteParam(ParamID pid) const {
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kPerBeatParams) {
        return static_cast<int>(pid - kActiveParamBase) == ActiveParamSlot::kActiveNoteIndex;
    }
    if (pid >= kParamBaseBeatParams && pid < kActiveParamBase) {
        int rel = static_cast<int>(pid - kParamBaseBeatParams);
        int slot = rel % kPerBeatParams;
        return slot == BeatParamSlot::kSlotNoteIndex;
    }
    return false;
}

static int normalizedToNoteIndex(ParamValue valueNormalized) {
    constexpr int min = 0;
    constexpr int max = 11;
    double v = min + valueNormalized * (max - min);
    v = std::clamp(v, static_cast<double>(min), static_cast<double>(max));
    return static_cast<int>(std::round(v));
}

tresult PLUGIN_API BeatController::getParamStringByValue(ParamID pid, ParamValue valueNormalized, String128 string) {
    if (isNoteParam(pid)) {
        static constexpr const char* kNoteNames[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int idx = normalizedToNoteIndex(valueNormalized);
        UString(string, str16BufferSize(String128)).fromAscii(kNoteNames[idx]);
        return kResultOk;
    }
    return EditControllerEx1::getParamStringByValue(pid, valueNormalized, string);
}

tresult PLUGIN_API BeatController::getParamValueByString(ParamID pid, TChar* string, ParamValue& valueNormalized) {
    if (isNoteParam(pid)) {
        UString noteStr(string, str16BufferSize(String128));
        char buffer[128]{};
        noteStr.toAscii(buffer, static_cast<int32>(sizeof(buffer)));
        std::string ascii(buffer);
        for (auto& ch : ascii) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }

        static constexpr const char* kNoteNames[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        for (int i = 0; i < 12; ++i) {
            if (ascii == kNoteNames[i]) {
                valueNormalized = static_cast<ParamValue>(i) / 11.0;
                return kResultOk;
            }
        }
    }
    return EditControllerEx1::getParamValueByString(pid, string, valueNormalized);
}

void BeatController::buildParamOrder() {
    paramOrder_.clear();
    paramOrder_.push_back(ParamIDs::kParamEffectEnabled);
    paramOrder_.push_back(ParamIDs::kParamBeatSelect);
    for (int b = 0; b < kMaxBeats; ++b) {
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotBars));
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotLoop));
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotBeats));
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotRotate));
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotNoteIndex));
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotOctave));
        paramOrder_.push_back(beatParamId(b, BeatParamSlot::kSlotLoud));
        paramOrder_.push_back(laneMuteParamId(b));
        paramOrder_.push_back(laneSoloParamId(b));
    }
}

ParamValue BeatController::defaultNormalized(ParamID pid) const {
    if (pid == ParamIDs::kParamEffectEnabled) return 0.0;
    if (pid == ParamIDs::kParamBeatSelect) return 0.0;

    if (pid < kParamBaseBeatParams) return 0.0;

    if (pid >= kActiveParamBase && pid < kLaneMuteBase) {
        int slot = static_cast<int>(pid - kActiveParamBase);
        switch (slot) {
            case ActiveParamSlot::kActiveBars: return (4.0 - 1.0) / (kMaxLoopLength - 1.0);
            case ActiveParamSlot::kActiveLoop: return (16.0 - 1.0) / (kMaxLoopLength - 1.0);
            case ActiveParamSlot::kActiveBeats: return 4.0 / kMaxLoopLength;
            case ActiveParamSlot::kActiveRotate: return 0.0;
            case ActiveParamSlot::kActiveNoteIndex: return 0.0;
            case ActiveParamSlot::kActiveOctave: return (2.0 - kMinOctave) / (kMaxOctave - kMinOctave);
            case ActiveParamSlot::kActiveLoud: return 0.0;
            default: break;
        }
        return 0.0;
    }

    if (pid >= kLaneMuteBase && pid < kLaneSoloBase) return 0.0;
    if (pid >= kLaneSoloBase && pid < kLaneActivityBase) return 0.0;
    if (pid >= kLaneActivityBase) return 0.0;

    int rel = static_cast<int>(pid - kParamBaseBeatParams);
    int beatIndex = rel / kPerBeatParams;
    int slot = rel % kPerBeatParams;

    switch (slot) {
        case BeatParamSlot::kSlotBars: return (4.0 - 1.0) / (kMaxLoopLength - 1.0);
        case BeatParamSlot::kSlotLoop: return (16.0 - 1.0) / (kMaxLoopLength - 1.0);
        case BeatParamSlot::kSlotBeats: return 4.0 / kMaxLoopLength;
        case BeatParamSlot::kSlotRotate: return 0.0;
        case BeatParamSlot::kSlotNoteIndex: return static_cast<ParamValue>(beatIndex % 12) / 11.0;
        case BeatParamSlot::kSlotOctave: return (2.0 - kMinOctave) / (kMaxOctave - kMinOctave);
        case BeatParamSlot::kSlotLoud: return 0.0;
        default: break;
    }
    return 0.0;
}

void BeatController::resetAllParams() {
    for (auto pid : paramOrder_) {
        ParamValue v = defaultNormalized(pid);
        beginEdit(pid);
        performEdit(pid, v);
        endEdit(pid);
        EditControllerEx1::setParamNormalized(pid, v);
    }
    syncActiveParams();
}

void BeatController::syncActiveParams() {
    if (syncingActive_) return;
    syncingActive_ = true;
    const int beatIndex = selectedBeatIndex();
    for (int slot = 0; slot < kPerBeatParams; ++slot) {
        ParamID src = beatParamId(beatIndex, slot);
        ParamID dst = activeParamId(slot);
        ParamValue v = getParamNormalized(src);
        EditControllerEx1::setParamNormalized(dst, v);
    }
    syncingActive_ = false;
    if (componentHandler) {
        componentHandler->restartComponent(Vst::kParamValuesChanged);
    }
}

int BeatController::selectedBeatIndex() {
    const ParamValue norm = getParamNormalized(ParamIDs::kParamBeatSelect);
    int beat = static_cast<int>(std::round(1.0 + norm * (kMaxBeats - 1)));
    beat = std::max(1, std::min(kMaxBeats, beat));
    return beat - 1;
}

} // namespace beatvst
