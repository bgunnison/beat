// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include "pluginterfaces/gui/iplugview.h"

namespace beatvst {

class BeatView : public Steinberg::IPlugView {
public:
    BeatView() = default;

    // IPlugView
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) SMTG_OVERRIDE { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API attached(void*, Steinberg::FIDString) SMTG_OVERRIDE { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API removed() SMTG_OVERRIDE { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API onWheel(float) SMTG_OVERRIDE { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API onKeyDown(char16, int16, int16, int16) SMTG_OVERRIDE { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API onKeyUp(char16, int16, int16, int16) SMTG_OVERRIDE { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) SMTG_OVERRIDE {
        if (!size) return Steinberg::kInvalidArgument;
        size->left = 0; size->top = 0; size->right = 640; size->bottom = 360;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect*) SMTG_OVERRIDE { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool) SMTG_OVERRIDE { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame*) SMTG_OVERRIDE { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API canResize() SMTG_OVERRIDE { return Steinberg::kResultFalse; }
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect*) SMTG_OVERRIDE { return Steinberg::kResultOk; }
};

} // namespace beatvst

