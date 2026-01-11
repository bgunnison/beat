// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#pragma once

#include "BeatEngine.h"
#include "BeatIDs.h"

#include "public.sdk/source/vst/vstaudioeffect.h"
#include <array>
#include <unordered_map>

namespace beatvst {

class BeatProcessor : public Steinberg::Vst::AudioEffect {
public:
    BeatProcessor();

    static Steinberg::FUnknown* createInstance(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new BeatProcessor()); }

    // VST3
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                     Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::uint32 PLUGIN_API getProcessContextRequirements() SMTG_OVERRIDE {
        return Steinberg::Vst::IProcessContextRequirements::kNeedTempo |
               Steinberg::Vst::IProcessContextRequirements::kNeedTransportState;
    }

protected:
    void handleParameterChanges(Steinberg::Vst::ProcessData& data);
    void applyNormalizedParam(Steinberg::Vst::ParamID pid, Steinberg::Vst::ParamValue value);
    void buildParamOrder();
    void syncEngineFromParams();
    void resetToDefaults();
    Steinberg::Vst::ParamValue defaultNormalized(Steinberg::Vst::ParamID pid) const;

    BeatEngine engine_;
    Steinberg::Vst::SampleRate sampleRate_{44100.0};
    double samplesPerTick_{(60.0 / 120.0) / 24.0}; // default 120 bpm, 24 ppq
    double sampleRemainder_{0.0};
    double startDelaySamples_{0.0};
    Steinberg::int64 globalTick_{0};
    bool wasPlaying_{false};
    std::vector<Steinberg::Vst::ParamID> paramOrder_;
    int currentSelected_{1};
    std::unordered_map<Steinberg::Vst::ParamID, double> paramState_;
    std::array<bool, kMaxBeats> laneMute_{};
    std::array<bool, kMaxBeats> laneSolo_{};
    std::array<int, kMaxBeats> activityCountdown_{};
    std::array<double, kMaxBeats> lastActivityValue_{};
};

} // namespace beatvst

