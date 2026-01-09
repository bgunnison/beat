// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "BeatController.h"
#include "BeatIDs.h"
#include "BeatProcessor.h"

#include "public.sdk/source/main/pluginfactory.h"

#ifdef BEAT_DEBUG_NAME
#define stringPluginName "Debug Beat"
#else
#define stringPluginName "Beat"
#endif

using namespace beatvst;
using namespace Steinberg;
using namespace Steinberg::Vst;

// Factory setup
BEGIN_FACTORY_DEF(kBeatVst3Vendor, kBeatVst3Url, kBeatVst3Email)

    DEF_CLASS2(INLINE_UID_FROM_FUID(kBeatProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               stringPluginName,
               Vst::kDistributable,
               PlugType::kInstrument, // Instrument with MIDI out
               kBeatVst3Version,
               kVstVersionString,
               BeatProcessor::createInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(kBeatControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               stringPluginName " Controller",
               0,
               "",
               kBeatVst3Version,
               kVstVersionString,
               BeatController::createInstance)

END_FACTORY

