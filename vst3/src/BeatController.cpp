// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "BeatController.h"

#include "BeatEngine.h"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/uidescription/delegationcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uidescription.h"
#include "vstgui/lib/controls/cautoanimation.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "base/source/fstreamer.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace beatvst {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr int32_t kBeatSelectButtonTagBase = 91;
constexpr int32_t kBeatSelectButtonTagLast = kBeatSelectButtonTagBase + kMaxBeats - 1;

bool isBeatSelectButtonTag(int32_t tag) {
    return tag >= kBeatSelectButtonTagBase && tag <= kBeatSelectButtonTagLast;
}

int beatSelectLaneIndexForTag(int32_t tag) {
    return isBeatSelectButtonTag(tag) ? static_cast<int>(tag - kBeatSelectButtonTagBase) : -1;
}

ParamValue beatSelectNormalizedForLane(int laneIndex) {
    if (kMaxBeats <= 1) return 0.0;
    const auto clampedLane = std::clamp(laneIndex, 0, kMaxBeats - 1);
    return static_cast<ParamValue>(clampedLane) / static_cast<ParamValue>(kMaxBeats - 1);
}

int beatSelectLaneIndexFromNormalized(ParamValue norm) {
    int beat = static_cast<int>(std::round(1.0 + norm * (kMaxBeats - 1)));
    beat = std::clamp(beat, 1, kMaxBeats);
    return beat - 1;
}

class BeatLaneSelectorController final : public VSTGUI::DelegationController {
public:
    BeatLaneSelectorController(VSTGUI::IController* baseController, BeatController& controller)
        : DelegationController(baseController), controller_(controller) {}

    ~BeatLaneSelectorController() override {
        for (auto* button : laneButtons_) {
            if (!button) continue;
            if (button->getListener() == this) {
                button->setListener(nullptr);
            }
            button->forget();
        }
    }

    VSTGUI::CView* verifyView(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
                              const VSTGUI::IUIDescription* description) override {
        if (auto* button = dynamic_cast<VSTGUI::CTextButton*>(view)) {
            const int laneIndex = beatSelectLaneIndexForTag(button->getTag());
            if (laneIndex >= 0 && laneButtons_[laneIndex] == nullptr) {
                button->setListener(this);
                button->remember();
                laneButtons_[laneIndex] = button;
                syncButtonState(*button, laneIndex == selectedLaneIndex());
            }
        }
        return DelegationController::verifyView(view, attributes, description);
    }

    void valueChanged(VSTGUI::CControl* pControl) override {
        const int laneIndex = laneIndexForControl(pControl);
        if (laneIndex < 0) {
            DelegationController::valueChanged(pControl);
            return;
        }

        syncButtons(laneIndex);
        const ParamValue normalized = beatSelectNormalizedForLane(laneIndex);
        controller_.setParamNormalized(ParamIDs::kParamBeatSelect, normalized);
        controller_.performEdit(ParamIDs::kParamBeatSelect, controller_.getParamNormalized(ParamIDs::kParamBeatSelect));
    }

    void controlBeginEdit(VSTGUI::CControl* pControl) override {
        if (laneIndexForControl(pControl) < 0) {
            DelegationController::controlBeginEdit(pControl);
            return;
        }
        controller_.beginEdit(ParamIDs::kParamBeatSelect);
    }

    void controlEndEdit(VSTGUI::CControl* pControl) override {
        if (laneIndexForControl(pControl) < 0) {
            DelegationController::controlEndEdit(pControl);
            return;
        }
        controller_.endEdit(ParamIDs::kParamBeatSelect);
    }

private:
    int laneIndexForControl(VSTGUI::CControl* control) const {
        return control ? beatSelectLaneIndexForTag(control->getTag()) : -1;
    }

    int selectedLaneIndex() const {
        return beatSelectLaneIndexFromNormalized(controller_.getParamNormalized(ParamIDs::kParamBeatSelect));
    }

    void syncButtonState(VSTGUI::CTextButton& button, bool selected) {
        button.setValue(selected ? 1.f : 0.f);
        button.invalid();
    }

    void syncButtons(int selectedIndex) {
        for (int laneIndex = 0; laneIndex < kMaxBeats; ++laneIndex) {
            auto* button = laneButtons_[laneIndex];
            if (!button) continue;
            syncButtonState(*button, laneIndex == selectedIndex);
        }
    }

    BeatController& controller_;
    std::array<VSTGUI::CTextButton*, kMaxBeats> laneButtons_ {};
};

} // namespace

tresult PLUGIN_API BeatController::initialize(FUnknown* context) {
    tresult res = EditControllerEx1::initialize(context);
    if (res != kResultOk) return res;

    // Effect on/off
    RangeParameter* effectEnabled = new RangeParameter(STR16("Mute All"), ParamIDs::kParamEffectEnabled, nullptr, 0.0, 1.0, 0.0);
    parameters.addParameter(effectEnabled);

    RangeParameter* globalSolo = new RangeParameter(STR16("Global Solo"), kParamGlobalSolo, nullptr, 0.0, 1.0, 0.0);
    globalSolo->setPrecision(0);
    parameters.addParameter(globalSolo);

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
        const int laneNumber = b + 1;
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Bars", beatParamId(b, BeatParamSlot::kSlotBars), 1, kMaxLoopLength, 4, 0, kRootUnitId);
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Loop", beatParamId(b, BeatParamSlot::kSlotLoop), 1, kMaxLoopLength, 16, 0, kRootUnitId);
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Beats", beatParamId(b, BeatParamSlot::kSlotBeats), 0, kMaxLoopLength, 4, 0, kRootUnitId);
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Rotate", beatParamId(b, BeatParamSlot::kSlotRotate), 0, kMaxLoopLength, 0, 0, kRootUnitId);
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Note", beatParamId(b, BeatParamSlot::kSlotNoteIndex), 0, 11, b % 12, 0, kRootUnitId);
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Octave", beatParamId(b, BeatParamSlot::kSlotOctave), kMinOctave, kMaxOctave, 2, 0, kRootUnitId);
        addBeatParam(b, "Lane " + std::to_string(laneNumber) + " Loud", beatParamId(b, BeatParamSlot::kSlotLoud), 0, 127, 0, 0, kRootUnitId);

        String128 laneMuteTitle{};
        UString(laneMuteTitle, str16BufferSize(String128)).fromAscii(("Lane " + std::to_string(laneNumber) + " Mute").c_str());
        auto* laneMute = new RangeParameter(laneMuteTitle, laneMuteParamId(b), nullptr, 0.0, 1.0, 0.0);
        laneMute->setPrecision(0);
        parameters.addParameter(laneMute);

        String128 laneSoloTitle{};
        UString(laneSoloTitle, str16BufferSize(String128)).fromAscii(("Lane " + std::to_string(laneNumber) + " Solo").c_str());
        auto* laneSolo = new RangeParameter(laneSoloTitle, laneSoloParamId(b), nullptr, 0.0, 1.0, 0.0);
        laneSolo->setPrecision(0);
        parameters.addParameter(laneSolo);

        String128 laneActivityTitle{};
        UString(laneActivityTitle, str16BufferSize(String128)).fromAscii(("Lane " + std::to_string(laneNumber) + " Activity").c_str());
        auto* laneActivity = new RangeParameter(laneActivityTitle, laneActivityParamId(b), nullptr, 0.0, 1.0, 0.0);
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

VSTGUI::IController* BeatController::createSubController(VSTGUI::UTF8StringPtr name, const VSTGUI::IUIDescription* description,
                                                         VSTGUI::VST3Editor* editor) {
    if (name && std::strcmp(name, "BeatLaneSelector") == 0) {
        return new BeatLaneSelectorController(editor, *this);
    }
    return VSTGUI::VST3EditorDelegate::createSubController(name, description, editor);
}

VSTGUI::CView* BeatController::verifyView(VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
                                          const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor* editor) {
    auto* autoAnim = dynamic_cast<VSTGUI::CAutoAnimation*>(view);
    if (autoAnim && !autoAnim->isWindowOpened()) {
        autoAnim->openWindow();
    }
    return view;
}

void BeatController::didOpen(VSTGUI::VST3Editor* editor) {
    if (autoExposed_) return;
    exposeAutomatableParams();
    autoExposed_ = true;
}

void BeatController::exposeAutomatableParams() {
    if (!componentHandler) return;
    const int32 count = parameters.getParameterCount();
    for (int32 i = 0; i < count; ++i) {
        auto* param = parameters.getParameterByIndex(i);
        if (!param) continue;
        const auto flags = param->getInfo().flags;
        if (flags & ParameterInfo::kIsHidden) continue;
        if (flags & ParameterInfo::kIsReadOnly) continue;
        const ParamID pid = param->getInfo().id;
        const ParamValue value = getParamNormalized(pid);
        ParamValue nudge = value + 1e-5;
        if (nudge > 1.0) nudge = value - 1e-5;
        if (nudge < 0.0) nudge = value;
        beginEdit(pid);
        performEdit(pid, nudge);
        performEdit(pid, value);
        endEdit(pid);
    }
}

tresult PLUGIN_API BeatController::getState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        ParamValue v = getParamNormalized(pid);
        if (!streamer.writeDouble(v)) return kResultFalse;
    }
    return kResultOk;
}

tresult PLUGIN_API BeatController::setComponentState(IBStream* state) {
    if (!state) return kInvalidArgument;
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double v = 0.0;
        if (!streamer.readDouble(v)) v = defaultNormalized(pid);
        if (pid != kParamGlobalSolo) {
            setParamNormalized(pid, v);
        }
    }
    syncGlobalSolo();
    syncActiveParams();
    if (componentHandler) {
        pushAllParamsToProcessor();
    } else {
        pendingProcessorSync_ = true;
    }
    return kResultOk;
}

tresult PLUGIN_API BeatController::setState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double v = 0.0;
        if (!streamer.readDouble(v)) v = defaultNormalized(pid);
        if (pid != kParamGlobalSolo) {
            setParamNormalized(pid, v);
        }
    }
    syncGlobalSolo();
    syncActiveParams();
    if (componentHandler) {
        pushAllParamsToProcessor();
    } else {
        pendingProcessorSync_ = true;
    }
    return kResultOk;
}

tresult PLUGIN_API BeatController::setParamNormalized(ParamID pid, ParamValue value) {
    if (pendingProcessorSync_ && componentHandler) {
        pushAllParamsToProcessor();
    }
    tresult res = EditControllerEx1::setParamNormalized(pid, value);
    if (pid == ParamIDs::kParamBeatSelect) {
        syncActiveParams();
        return res;
    }
    if (pid == ParamIDs::kParamEffectEnabled) {
        applyGlobalMuteToLanes(value > 0.5);
        return res;
    }
    if (pid == kParamGlobalSolo) {
        if (value <= 0.5) {
            applyGlobalSoloClear();
        } else {
            syncGlobalSolo();
        }
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
    if (pid >= kParamBaseBeatParams && pid < kActiveParamBase) {
        if (!syncingActive_) {
            const int rel = static_cast<int>(pid - kParamBaseBeatParams);
            const int beatIndex = rel / kPerBeatParams;
            const int slot = rel % kPerBeatParams;
            if (beatIndex == selectedBeatIndex()) {
                const ParamID activeId = activeParamId(static_cast<ActiveParamSlot>(slot));
                syncingActive_ = true;
                EditControllerEx1::setParamNormalized(activeId, value);
                if (componentHandler) {
                    componentHandler->beginEdit(activeId);
                    componentHandler->performEdit(activeId, value);
                    componentHandler->endEdit(activeId);
                }
                syncingActive_ = false;
            }
        }
        return res;
    }
    if (pid >= kLaneSoloBase && pid < kLaneActivityBase) {
        syncGlobalSolo();
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

tresult PLUGIN_API BeatController::setComponentHandler(IComponentHandler* handler) {
    tresult res = EditControllerEx1::setComponentHandler(handler);
    if (res == kResultOk && handler && !autoExposed_) {
        exposeAutomatableParams();
        autoExposed_ = true;
    }
    return res;
}

void BeatController::pushAllParamsToProcessor() {
    if (!componentHandler || pushingToProcessor_) return;
    pushingToProcessor_ = true;
    pendingProcessorSync_ = false;
    for (auto pid : paramOrder_) {
        if (pid == ParamIDs::kParamReset) continue;
        if (pid >= kActiveParamBase && pid < kLaneMuteBase) continue;
        if (pid >= kLaneActivityBase) continue;
        if (pid == kParamGlobalSolo) continue;
        const ParamValue v = getParamNormalized(pid);
        componentHandler->beginEdit(pid);
        componentHandler->performEdit(pid, v);
        componentHandler->endEdit(pid);
    }
    pushingToProcessor_ = false;
}

void BeatController::applyGlobalMuteToLanes(bool muted) {
    if (!componentHandler) return;
    for (int b = 0; b < kMaxBeats; ++b) {
        const ParamID pid = laneMuteParamId(b);
        const ParamValue v = muted ? 1.0 : 0.0;
        EditControllerEx1::setParamNormalized(pid, v);
        componentHandler->beginEdit(pid);
        componentHandler->performEdit(pid, v);
        componentHandler->endEdit(pid);
    }
}

void BeatController::applyGlobalSoloClear() {
    if (!componentHandler) return;
    for (int b = 0; b < kMaxBeats; ++b) {
        const ParamID pid = laneSoloParamId(b);
        EditControllerEx1::setParamNormalized(pid, 0.0);
        componentHandler->beginEdit(pid);
        componentHandler->performEdit(pid, 0.0);
        componentHandler->endEdit(pid);
    }
    EditControllerEx1::setParamNormalized(kParamGlobalSolo, 0.0);
    componentHandler->beginEdit(kParamGlobalSolo);
    componentHandler->performEdit(kParamGlobalSolo, 0.0);
    componentHandler->endEdit(kParamGlobalSolo);
}

void BeatController::syncGlobalSolo() {
    bool anySolo = false;
    for (int b = 0; b < kMaxBeats; ++b) {
        const ParamID pid = laneSoloParamId(b);
        if (getParamNormalized(pid) > 0.5) { anySolo = true; break; }
    }
    const ParamValue v = anySolo ? 1.0 : 0.0;
    EditControllerEx1::setParamNormalized(kParamGlobalSolo, v);
    if (componentHandler) {
        componentHandler->beginEdit(kParamGlobalSolo);
        componentHandler->performEdit(kParamGlobalSolo, v);
        componentHandler->endEdit(kParamGlobalSolo);
    }
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
    paramOrder_.push_back(kParamGlobalSolo);
}

ParamValue BeatController::defaultNormalized(ParamID pid) const {
    if (pid == ParamIDs::kParamEffectEnabled) return 0.0;
    if (pid == kParamGlobalSolo) return 0.0;
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
    return beatSelectLaneIndexFromNormalized(getParamNormalized(ParamIDs::kParamBeatSelect));
}

} // namespace beatvst

