// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "BeatProcessor.h"

#include "BeatController.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#ifdef BEAT_DEBUG_NAME
#include <fstream>
#endif

namespace beatvst {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

void addOutputParamChange(IParameterChanges* changes, ParamID pid, ParamValue value, int32 sampleOffset) {
    if (!changes) return;
    int32 index = 0;
    IParamValueQueue* queue = changes->addParameterData(pid, index);
    if (!queue) return;
    int32 pointIndex = 0;
    queue->addPoint(sampleOffset, value, pointIndex);
}

} // namespace

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
        engine_.setMuted(value > 0.5);
        const bool muted = value > 0.5;
        for (int b = 0; b < kMaxBeats; ++b) {
            laneMute_[static_cast<size_t>(b)] = muted;
            engine_.setLaneMute(b, muted);
            paramState_[laneMuteParamId(b)] = muted ? 1.0 : 0.0;
        }
        return;
    }

    if (pid == kParamGlobalSolo) {
        paramState_[pid] = value;
        if (value <= 0.5) {
            for (int b = 0; b < kMaxBeats; ++b) {
                laneSolo_[static_cast<size_t>(b)] = false;
                engine_.setLaneSolo(b, false);
                paramState_[laneSoloParamId(b)] = 0.0;
            }
        }
        return;
    }

    if (pid == ParamIDs::kParamReset) {
        if (value > 0.5) resetToDefaults();
        return;
    }

    if (pid >= kLaneMuteBase && pid < kLaneSoloBase) {
        int beatIndex = static_cast<int>(pid - kLaneMuteBase);
        if (beatIndex >= 0 && beatIndex < kMaxBeats) {
            const bool muted = value > 0.5;
            laneMute_[static_cast<size_t>(beatIndex)] = muted;
            engine_.setLaneMute(beatIndex, muted);
            paramState_[pid] = muted ? 1.0 : 0.0;
        }
        return;
    }

    if (pid >= kLaneSoloBase && pid < kLaneActivityBase) {
        int beatIndex = static_cast<int>(pid - kLaneSoloBase);
        if (beatIndex >= 0 && beatIndex < kMaxBeats) {
            const bool solo = value > 0.5;
            laneSolo_[static_cast<size_t>(beatIndex)] = solo;
            engine_.setLaneSolo(beatIndex, solo);
            paramState_[pid] = solo ? 1.0 : 0.0;
            bool anySolo = false;
            for (bool s : laneSolo_) {
                if (s) { anySolo = true; break; }
            }
            paramState_[kParamGlobalSolo] = anySolo ? 1.0 : 0.0;
        }
        return;
    }

    int beatIndex = -1;
    int slot = -1;
    if (pid >= kActiveParamBase && pid < kActiveParamBase + kPerBeatParams) {
        return;
    }

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
        case BeatParamSlot::kSlotNoteIndex: name = "NoteIndex"; min = 0; max = 11; break;
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
        if (pid != ParamIDs::kParamBeatSelect) continue;
        int32 points = queue->getPointCount();
        ParamValue value = 0;
        int32 offset = 0;
        queue->getPoint(points - 1, offset, value);
        applyNormalizedParam(pid, value);
    }
    for (int32 i = 0; i < count; ++i) {
        IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
        if (!queue) continue;
        ParamID pid = queue->getParameterId();
        if (pid == ParamIDs::kParamBeatSelect) continue;
        int32 points = queue->getPointCount();
        ParamValue value = 0;
        int32 offset = 0;
        queue->getPoint(points - 1, offset, value);
        applyNormalizedParam(pid, value);
    }
}

ParamValue BeatProcessor::defaultNormalized(ParamID pid) const {
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

void BeatProcessor::resetToDefaults() {
    for (auto pid : paramOrder_) {
        applyNormalizedParam(pid, defaultNormalized(pid));
    }
    sampleRemainder_ = 0.0;
    globalTick_ = 0;
    activityCountdown_.fill(0);
    lastActivityValue_.fill(0.0);
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
        paramOrder_.push_back(laneMuteParamId(b));
        paramOrder_.push_back(laneSoloParamId(b));
    }
    paramOrder_.push_back(kParamGlobalSolo);

    // Seed defaults so save/restore matches initial behavior.
    paramState_.clear();
    for (auto pid : paramOrder_) {
        paramState_[pid] = defaultNormalized(pid);
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
        startDelaySamples_ = 0.0;
        globalTick_ = -1;
        int currentSelected = engine_.selectedBeat();
        for (int b = 0; b < kMaxBeats; ++b) {
            engine_.selectBeat(b + 1);
            BeatParams p = engine_.getBeatParams(b);
            engine_.setBeatParam("Bars", p.bars);
            engine_.setBeatParam("Loop", p.loop);
            engine_.setBeatParam("Beats", p.beats);
            engine_.setBeatParam("Rotate", p.rotate);
            engine_.setBeatParam("Octave", p.octave);
            engine_.setBeatParam("NoteIndex", p.noteIndex);
            engine_.setBeatParam("Loud", p.loud);
        }
        engine_.selectBeat(currentSelected);
        return kResultOk;
    }
    if (!wasPlaying_) {
        startDelaySamples_ = 0.0;
        if (data.processContext &&
            (data.processContext->state & ProcessContext::kProjectTimeMusicValid)) {
            double ppq = data.processContext->projectTimeMusic;
            if (data.processContext->state & ProcessContext::kTimeSigValid) {
                const int32 num = data.processContext->timeSigNumerator > 0 ? data.processContext->timeSigNumerator : 4;
                const int32 den = data.processContext->timeSigDenominator > 0 ? data.processContext->timeSigDenominator : 4;
                const double barLengthQ = num * (4.0 / static_cast<double>(den));
                const double nearestBar = std::round(ppq / barLengthQ) * barLengthQ;
                const double snapThresholdQ = 1.0 / 96.0;
                if (std::abs(ppq - nearestBar) < snapThresholdQ) {
                    ppq = nearestBar;
                }
            }
            const double tickPos = ppq * 24.0;
            const double tickFloor = std::floor(tickPos);
            globalTick_ = static_cast<int64>(tickFloor) - 1;
            const double tickFrac = tickPos - tickFloor;
            if (tickFrac < 1e-4 || ppq < 1e-4) {
                // Snap to the bar start: fire the first tick at sample offset 0.
                sampleRemainder_ = samplesPerTick_;
            } else {
                sampleRemainder_ = tickFrac * samplesPerTick_;
            }
        } else {
            sampleRemainder_ = 0.0;
        }

#ifdef BEAT_DEBUG_NAME
        if (data.processContext) {
            const double ppqLog = (data.processContext->state & ProcessContext::kProjectTimeMusicValid)
                ? data.processContext->projectTimeMusic
                : -1.0;
            const double tempoLog = (data.processContext->state & ProcessContext::kTempoValid)
                ? data.processContext->tempo
                : -1.0;
            const int32 numLog = (data.processContext->state & ProcessContext::kTimeSigValid)
                ? data.processContext->timeSigNumerator
                : 0;
            const int32 denLog = (data.processContext->state & ProcessContext::kTimeSigValid)
                ? data.processContext->timeSigDenominator
                : 0;
            std::ofstream log("C:\\projects\\ableplugs\\beat\\beat_start.log", std::ios::app);
            log << "[Beat] start: ppq=" << ppqLog
                << " tempo=" << tempoLog
                << " sig=" << numLog << "/" << denLog
                << " samplesPerTick=" << samplesPerTick_
                << " sampleRemainder=" << sampleRemainder_
                << std::endl;
        }
#endif
        globalTick_ = -1;
        engine_.resetTiming();
    }
    wasPlaying_ = true;

    double samplesToProcess = static_cast<double>(data.numSamples);
    double cursor = 0.0;
    if (startDelaySamples_ > 0.0) {
        if (startDelaySamples_ >= samplesToProcess) {
            startDelaySamples_ -= samplesToProcess;
            return kResultOk;
        }
        cursor = startDelaySamples_;
        startDelaySamples_ = 0.0;
        sampleRemainder_ = 0.0;
    }
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
                if (ev.beatIndex >= 0 && ev.beatIndex < kMaxBeats) {
                    activityCountdown_[static_cast<size_t>(ev.beatIndex)] = 2;
                }
            } else {
                e.type = Event::kNoteOffEvent;
                e.noteOff.channel = 0;
                e.noteOff.pitch = ev.note;
                e.noteOff.velocity = 0.0f;
            }
            outEvents->addEvent(e);
        }

        for (int i = 0; i < kMaxBeats; ++i) {
            double activityValue = 0.0;
            if (activityCountdown_[static_cast<size_t>(i)] > 0) {
                activityCountdown_[static_cast<size_t>(i)] -= 1;
                activityValue = 1.0;
            }
            if (activityValue != lastActivityValue_[static_cast<size_t>(i)]) {
                addOutputParamChange(data.outputParameterChanges, laneActivityParamId(i), activityValue, sampleOffset);
                lastActivityValue_[static_cast<size_t>(i)] = activityValue;
            }
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
        if (!streamer.readDouble(v)) v = defaultNormalized(pid);
        if (pid != kParamGlobalSolo) {
            applyNormalizedParam(pid, v);
        }
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

