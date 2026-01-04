#include "BeatProcessor.h"

#include "BeatController.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace beatvst {
using namespace Steinberg;
using namespace Steinberg::Vst;

BeatProcessor::BeatProcessor() {
    setControllerClass(kBeatControllerUID);
    setProcessing(true);
    buildParamOrder();
}

tresult PLUGIN_API BeatProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;

    // Provide a silent stereo audio output so hosts that expect audio on instruments will load us.
    addAudioOutput(STR16("Audio Out"), SpeakerArr::kStereo);
    // MIDI only: one event input (optional) and one event output.
    addEventInput(STR16("MIDI In"), 1);
    addEventOutput(STR16("MIDI Out"), 1);
    return kResultOk;
}

tresult PLUGIN_API BeatProcessor::terminate() {
    return AudioEffect::terminate();
}

tresult PLUGIN_API BeatProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                     SpeakerArrangement* outputs, int32 numOuts) {
    // 0 audio ins, 1 stereo audio out.
    if (numIns != 0 || numOuts != 1) return kResultFalse;
    if (outputs[0] != SpeakerArr::kStereo) return kResultFalse;
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API BeatProcessor::setupProcessing(ProcessSetup& setup) {
    sampleRate_ = setup.sampleRate;
    buildParamOrder();
    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API BeatProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
    if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64) return kResultOk;
    return kResultFalse;
}

void BeatProcessor::applyNormalizedParam(ParamID pid, ParamValue value) {
    auto normToInt = [&](double norm, int min, int max) -> int {
        double v = min + norm * (max - min);
        v = std::clamp(v, static_cast<double>(min), static_cast<double>(max));
        return static_cast<int>(std::round(v));
    };

    if (pid == ParamIDs::kParamBeatSelect) {
        currentSelected_ = normToInt(value, 1, kMaxBeats);
        engine_.selectBeat(currentSelected_);
        paramState_[pid] = value;
        return;
    }

    if (pid == ParamIDs::kParamEffectEnabled) {
        paramState_[pid] = value;
        return;
    }

    if (pid == ParamIDs::kParamReset) {
        if (value > 0.5) resetToDefaults();
        return;
    }

    int beatIndex = -1;
    int slot = -1;
    if (pid >= kParamBaseBeatParams && pid < kActiveParamBase) {
        int rel = static_cast<int>(pid - kParamBaseBeatParams);
        beatIndex = rel / kPerBeatParams;
        slot = rel % kPerBeatParams;
    } else if (pid >= kActiveParamBase && pid < kActiveParamBase + kPerBeatParams) {
        beatIndex = currentSelected_ - 1;
        slot = static_cast<int>(pid - kActiveParamBase);
        if (beatIndex < 0) beatIndex = 0;
    } else {
        return;
    }
    if (beatIndex < 0 || beatIndex >= kMaxBeats) return;

    const char* name = nullptr;
    int min = 0;
    int max = 0;
    switch (slot) {
        case BeatParamSlot::kSlotBars: name = "Bars"; min = 1; max = kMaxLoopLength; break;
        case BeatParamSlot::kSlotLoop: name = "Loop"; min = 1; max = kMaxLoopLength; break;
        case BeatParamSlot::kSlotBeats: name = "Beats"; min = 0; max = kMaxLoopLength; break;
        case BeatParamSlot::kSlotRotate: name = "Rotate"; min = 0; max = kMaxLoopLength; break;
        case BeatParamSlot::kSlotNoteIndex: name = "Note"; min = 0; max = 11; break;
        case BeatParamSlot::kSlotOctave: name = "Octave"; min = kMinOctave; max = kMaxOctave; break;
        case BeatParamSlot::kSlotLoud: name = "Loud"; min = 0; max = 127; break;
        default: break;
    }
    if (!name) return;

    int currentSelected = engine_.selectedBeat();
    engine_.selectBeat(beatIndex + 1);
    engine_.setBeatParam(name, normToInt(value, min, max));
    engine_.selectBeat(currentSelected);
    paramState_[beatParamId(beatIndex, slot)] = value;
}

void BeatProcessor::handleParameterChanges(ProcessData& data) {
    if (!data.inputParameterChanges) return;
    int32 count = data.inputParameterChanges->getParameterCount();
    for (int32 i = 0; i < count; ++i) {
        IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
        if (!queue) continue;
        ParamID pid = queue->getParameterId();
        int32 points = queue->getPointCount();
        ParamValue value = 0;
        int32 offset = 0;
        queue->getPoint(points - 1, offset, value);
        applyNormalizedParam(pid, value);
    }
}

ParamValue BeatProcessor::defaultNormalized(ParamID pid) const {
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

void BeatProcessor::resetToDefaults() {
    for (auto pid : paramOrder_) {
        applyNormalizedParam(pid, defaultNormalized(pid));
    }
    sampleRemainder_ = 0.0;
    globalTick_ = 0;
}

void BeatProcessor::syncEngineFromParams() {
    // No-op: we apply all params directly in applyNormalizedParam.
}

void BeatProcessor::buildParamOrder() {
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

    // Seed defaults so save/restore matches initial behavior.
    paramState_.clear();
    paramState_[ParamIDs::kParamEffectEnabled] = 1.0;
    paramState_[ParamIDs::kParamBeatSelect] = 0.0;
    for (int b = 0; b < kMaxBeats; ++b) {
        paramState_[beatParamId(b, BeatParamSlot::kSlotBars)] = (4.0 - 1.0) / (kMaxLoopLength - 1.0);
        paramState_[beatParamId(b, BeatParamSlot::kSlotLoop)] = (16.0 - 1.0) / (kMaxLoopLength - 1.0);
        paramState_[beatParamId(b, BeatParamSlot::kSlotBeats)] = 4.0 / kMaxLoopLength;
        paramState_[beatParamId(b, BeatParamSlot::kSlotRotate)] = 0.0;
        paramState_[beatParamId(b, BeatParamSlot::kSlotNoteIndex)] = 0.0;
        paramState_[beatParamId(b, BeatParamSlot::kSlotOctave)] = (4.0 - kMinOctave) / (kMaxOctave - kMinOctave);
        paramState_[beatParamId(b, BeatParamSlot::kSlotLoud)] = 1.0;
    }
}

tresult PLUGIN_API BeatProcessor::process(ProcessData& data) {
    handleParameterChanges(data);

    // Keep audio silent.
    if (data.numOutputs > 0 && data.outputs) {
        for (int32 bus = 0; bus < data.numOutputs; ++bus) {
            auto& out = data.outputs[bus];
            if (out.channelBuffers32 && data.symbolicSampleSize == kSample32) {
                for (uint32 c = 0; c < out.numChannels; ++c) {
                    std::fill_n(out.channelBuffers32[c], data.numSamples, 0.0f);
                }
            }
            if (out.channelBuffers64 && data.symbolicSampleSize == kSample64) {
                for (uint32 c = 0; c < out.numChannels; ++c) {
                    std::fill_n(out.channelBuffers64[c], data.numSamples, 0.0);
                }
            }
        }
    }

    if (!data.processContext || !(data.processContext->state & ProcessContext::kTempoValid)) {
        // Fallback to default tempo if host does not provide it.
        samplesPerTick_ = (sampleRate_ * 60.0) / (120.0 * 24.0);
    } else {
        const double tempo = data.processContext->tempo > 0.0 ? data.processContext->tempo : 120.0;
        samplesPerTick_ = (sampleRate_ * 60.0) / (tempo * 24.0);
    }

    if (data.numSamples <= 0) {
        return kResultOk;
    }

    IEventList* outEvents = data.outputEvents;
    if (!outEvents) return kResultOk;

    const bool playing = data.processContext && (data.processContext->state & ProcessContext::kPlaying);
    if (!playing) {
        if (wasPlaying_) {
            std::vector<BeatEvent> purgeEvents;
            engine_.purgeAll(purgeEvents);
            for (const auto& ev : purgeEvents) {
                Event e{};
                e.sampleOffset = 0;
                e.type = Event::kNoteOffEvent;
                e.noteOff.channel = 0;
                e.noteOff.pitch = ev.note;
                e.noteOff.velocity = 0.0f;
                outEvents->addEvent(e);
            }
        }
        wasPlaying_ = false;
        sampleRemainder_ = 0.0;
        return kResultOk;
    }
    wasPlaying_ = true;

    double samplesToProcess = static_cast<double>(data.numSamples);
    double cursor = 0.0;
    double samplesUntilTick = samplesPerTick_ - sampleRemainder_;

    while (cursor + samplesUntilTick <= samplesToProcess) {
        int32 sampleOffset = static_cast<int32>(std::min(cursor + samplesUntilTick, samplesToProcess - 1));
        cursor += samplesUntilTick;
        sampleRemainder_ = 0.0;
        globalTick_ += 1;

        std::vector<BeatEvent> events;
        engine_.processTick(static_cast<int>(globalTick_), events);
        for (const auto& ev : events) {
            Event e{};
            e.sampleOffset = sampleOffset;
            if (ev.noteOn) {
                e.type = Event::kNoteOnEvent;
                e.noteOn.channel = 0;
                e.noteOn.pitch = ev.note;
                e.noteOn.velocity = ev.velocity / 127.f;
                e.noteOn.length = 0;
            } else {
                e.type = Event::kNoteOffEvent;
                e.noteOff.channel = 0;
                e.noteOff.pitch = ev.note;
                e.noteOff.velocity = 0.0f;
            }
            outEvents->addEvent(e);
        }

        samplesUntilTick = samplesPerTick_;
    }

    sampleRemainder_ += samplesToProcess - cursor;
    return kResultOk;
}

tresult PLUGIN_API BeatProcessor::setState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double v = 0.0;
        if (!streamer.readDouble(v)) break;
        applyNormalizedParam(pid, v);
    }
    return kResultOk;
}

tresult PLUGIN_API BeatProcessor::getState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    for (auto pid : paramOrder_) {
        double v = 0.0;
        auto it = paramState_.find(pid);
        if (it != paramState_.end()) v = it->second;
        streamer.writeDouble(v);
    }
    return kResultOk;
}

} // namespace beatvst
