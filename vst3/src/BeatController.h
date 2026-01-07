#pragma once

#include "BeatIDs.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace beatvst {

class BeatController : public Steinberg::Vst::EditControllerEx1 {
public:
    BeatController() = default;
    static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IEditController*>(new BeatController()); }

    // VST3
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID pid, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID pid, Steinberg::Vst::ParamValue valueNormalized,
                                                        Steinberg::Vst::String128 string) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID pid, Steinberg::Vst::TChar* string,
                                                        Steinberg::Vst::ParamValue& valueNormalized) SMTG_OVERRIDE;

private:
    void buildParamOrder();
    Steinberg::Vst::ParamValue defaultNormalized(Steinberg::Vst::ParamID pid) const;
    void resetAllParams();
    void syncActiveParams();
    int selectedBeatIndex();
    bool isNoteParam(Steinberg::Vst::ParamID pid) const;
    bool syncingActive_{false};
    std::vector<Steinberg::Vst::ParamID> paramOrder_;
};

} // namespace beatvst
