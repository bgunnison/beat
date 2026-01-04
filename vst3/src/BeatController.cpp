#include "BeatController.h"

#include "BeatEngine.h"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uidescription.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <string>

namespace beatvst {
using namespace Steinberg;
using namespace Steinberg::Vst;

tresult PLUGIN_API BeatController::initialize(FUnknown* context) {
    tresult res = EditControllerEx1::initialize(context);
    if (res != kResultOk) return res;

    // Effect on/off
    RangeParameter* effectEnabled = new RangeParameter(STR16("Effect Enabled"), ParamIDs::kParamEffectEnabled, nullptr, 0.0, 1.0, 1.0);
    parameters.addParameter(effectEnabled);

    RangeParameter* beatSelect = new RangeParameter(STR16("Beat Select"), ParamIDs::kParamBeatSelect, nullptr, 1.0, static_cast<ParamValue>(kMaxBeats), 1.0);
    beatSelect->setPrecision(0);
    parameters.addParameter(beatSelect);

    RangeParameter* resetParam = new RangeParameter(STR16("Reset"), ParamIDs::kParamReset, nullptr, 0.0, 1.0, 0.0);
    resetParam->setPrecision(0);
    parameters.addParameter(resetParam);

    UnitID activeUnitId = static_cast<UnitID>(kMaxBeats + 1);
    String128 activeUnitName{};
    UString(activeUnitName, str16BufferSize(String128)).fromAscii("Selected Beat");
    addUnit(new Unit(activeUnitName, activeUnitId));

    auto addActiveParam = [&](const std::string& label, ParamID id, double min, double max, double def, int precision) {
        String128 title{};
        UString(title, str16BufferSize(String128)).fromAscii(label.c_str());
        auto* p = new RangeParameter(title, id, nullptr, min, max, def);
        p->setPrecision(precision);
        p->setUnitID(activeUnitId);
        parameters.addParameter(p);
    };

    addActiveParam("Bars", activeParamId(ActiveParamSlot::kActiveBars), 1, kMaxLoopLength, 4, 0);
    addActiveParam("Loop", activeParamId(ActiveParamSlot::kActiveLoop), 1, kMaxLoopLength, 16, 0);
    addActiveParam("Beats", activeParamId(ActiveParamSlot::kActiveBeats), 0, kMaxLoopLength, 4, 0);
    addActiveParam("Rotate", activeParamId(ActiveParamSlot::kActiveRotate), 0, kMaxLoopLength, 0, 0);
    addActiveParam("NoteIndex", activeParamId(ActiveParamSlot::kActiveNoteIndex), 0, 11, 0, 0);
    addActiveParam("Octave", activeParamId(ActiveParamSlot::kActiveOctave), kMinOctave, kMaxOctave, 4, 0);
    addActiveParam("Loud", activeParamId(ActiveParamSlot::kActiveLoud), 0, 127, 127, 0);

    auto addBeatParam = [&](int beat, const std::string& label, ParamID id, double min, double max, double def, int precision, UnitID unitId) {
        String128 title{};
        UString(title, str16BufferSize(String128)).fromAscii(label.c_str());
        auto* p = new RangeParameter(title, id, nullptr, min, max, def);
        p->setPrecision(precision);
        p->setUnitID(unitId);
        parameters.addParameter(p);
    };

    for (int b = 0; b < kMaxBeats; ++b) {
        // Create a Unit for each generator to group parameters in the host
        UnitID unitId = static_cast<UnitID>(b + 1);
        String128 unitName{};
        UString(unitName, str16BufferSize(String128)).fromAscii(("Generator " + std::to_string(b + 1)).c_str());
        addUnit(new Unit(unitName, unitId));

        addBeatParam(b, "Bars", beatParamId(b, BeatParamSlot::kSlotBars), 1, kMaxLoopLength, 4, 0, unitId);
        addBeatParam(b, "Loop", beatParamId(b, BeatParamSlot::kSlotLoop), 1, kMaxLoopLength, 16, 0, unitId);
        addBeatParam(b, "Beats", beatParamId(b, BeatParamSlot::kSlotBeats), 0, kMaxLoopLength, 4, 0, unitId);
        addBeatParam(b, "Rotate", beatParamId(b, BeatParamSlot::kSlotRotate), 0, kMaxLoopLength, 0, 0, unitId);
        addBeatParam(b, "NoteIndex", beatParamId(b, BeatParamSlot::kSlotNoteIndex), 0, 11, 0, 0, unitId);
        addBeatParam(b, "Octave", beatParamId(b, BeatParamSlot::kSlotOctave), kMinOctave, kMaxOctave, 4, 0, unitId);
        addBeatParam(b, "Loud", beatParamId(b, BeatParamSlot::kSlotLoud), 0, 127, 127, 0, unitId);
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
    }
}

ParamValue BeatController::defaultNormalized(ParamID pid) const {
    if (pid == ParamIDs::kParamEffectEnabled) return 1.0;
    if (pid == ParamIDs::kParamBeatSelect) return 0.0;

    if (pid < kParamBaseBeatParams) return 0.0;
    int rel = static_cast<int>(pid - kParamBaseBeatParams);
    int slot = rel % kPerBeatParams;

    switch (slot) {
        case BeatParamSlot::kSlotBars: return (4.0 - 1.0) / (kMaxLoopLength - 1.0);
        case BeatParamSlot::kSlotLoop: return (16.0 - 1.0) / (kMaxLoopLength - 1.0);
        case BeatParamSlot::kSlotBeats: return 4.0 / kMaxLoopLength;
        case BeatParamSlot::kSlotRotate: return 0.0;
        case BeatParamSlot::kSlotNoteIndex: return 0.0;
        case BeatParamSlot::kSlotOctave: return (4.0 - kMinOctave) / (kMaxOctave - kMinOctave);
        case BeatParamSlot::kSlotLoud: return 1.0;
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
}

int BeatController::selectedBeatIndex() {
    const ParamValue norm = getParamNormalized(ParamIDs::kParamBeatSelect);
    int beat = static_cast<int>(std::round(1.0 + norm * (kMaxBeats - 1)));
    beat = std::max(1, std::min(kMaxBeats, beat));
    return beat - 1;
}

} // namespace beatvst
