// Copyright (c) 2026 Brian R. Gunnison
// MIT License
#include "BeatEngine.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>

namespace beatvst {

namespace {

// Matches the Python NOTES table.
constexpr const char* kNotes[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
constexpr int kNotesCount = 12;

std::vector<int> bjorklund(int steps, int pulses) {
    if (pulses > steps) throw std::invalid_argument("pulses > steps");
    std::vector<int> pattern;
    if (pulses == 0) {
        pattern.assign(static_cast<size_t>(steps), 0);
        return pattern;
    }

    std::vector<int> counts;
    std::vector<int> remainders;
    remainders.push_back(pulses);
    int divisor = steps - pulses;
    int level = 0;
    while (true) {
        counts.push_back(divisor / remainders[static_cast<size_t>(level)]);
        remainders.push_back(divisor % remainders[static_cast<size_t>(level)]);
        divisor = remainders[static_cast<size_t>(level)];
        level++;
        if (remainders[static_cast<size_t>(level)] < 2) break;
    }
    counts.push_back(divisor);

    std::function<void(int)> build = [&](int l) {
        if (l == -1) {
            pattern.push_back(0);
        } else if (l == -2) {
            pattern.push_back(1);
        } else {
            for (int i = 0; i < counts[static_cast<size_t>(l)]; ++i) build(l - 1);
            if (remainders[static_cast<size_t>(l)] != 0) build(l - 2);
        }
    };
    build(level);
    auto it = std::find(pattern.begin(), pattern.end(), 1);
    if (it != pattern.end()) {
        std::rotate(pattern.begin(), it, pattern.end());
    }
    return pattern;
}

} // namespace

uint8_t noteIndexToMidi(int octave, int noteIndex) {
    if (noteIndex < 0 || noteIndex >= kNotesCount) return 60;
    int number = noteIndex + ((octave + 1) * 12);
    number = std::clamp(number, 0, 127);
    return static_cast<uint8_t>(number);
}

Beat::Beat(int index) : index_(index) {
    params_.noteIndex = index_ % kNotesCount;
    rebuildNotes();
    rebuildPattern();
}

void Beat::rebuildNotes() {
    noteOn_ = noteIndexToMidi(params_.octave, params_.noteIndex);
    noteOff_ = noteOn_;
    updateNotes_ = false;
}

void Beat::rebuildPattern() {
    if (params_.loop < params_.beats) {
        mute_ = true;
        updatePattern_ = false;
        return;
    }

    try {
        truths_ = bjorklund(params_.loop, params_.beats);
    } catch (...) {
        truths_.assign(static_cast<size_t>(params_.loop), 0);
    }

    if (params_.rotate != 0 && !truths_.empty()) {
        int r = params_.rotate % static_cast<int>(truths_.size());
        if (r < 0) r += static_cast<int>(truths_.size());
        std::rotate(truths_.begin(), truths_.end() - r, truths_.end());
    }

    int ticksPerBar = params_.bars * 4 * 24;
    tickCountdown_ = 0;
    truthIndex_ = 0;
    // run() is called every tick; we only act every 6 ticks.
    // So beat interval is (ticksPerBar/6) / loop.
    int ticksPerStep = static_cast<int>(std::round((ticksPerBar / 6.0) / params_.loop));
    stepTicks_ = std::max(1, ticksPerStep);
    tickCountdown_ = stepTicks_;
    updatePattern_ = false;
    checkMute();
}

void Beat::checkMute() {
    mute_ = (params_.beats == 0) || (params_.loud == 0);
    if (mute_) muted_ = false;
}

bool Beat::setParam(const char* name, int value) {
    const std::string key(name);
    if (key == "Bars") params_.bars = value;
    else if (key == "Loop") params_.loop = value;
    else if (key == "Beats") params_.beats = value;
    else if (key == "Rotate") params_.rotate = value;
    else if (key == "Octave") params_.octave = value;
    else if (key == "Note" || key == "NoteIndex") params_.noteIndex = value;
    else if (key == "Loud") params_.loud = value;
    else return false;

    if (key == "Octave" || key == "Note" || key == "NoteIndex") {
        updateNotes_ = true;
    } else if (key == "Loud") {
        // Loudness should not rebuild the pattern; it only affects velocity/mute.
        updatePattern_ = false;
    } else {
        updatePattern_ = true;
    }
    checkMute();
    return true;
}

void Beat::setParams(const BeatParams& p) {
    params_ = p;
    updateNotes_ = true;
    updatePattern_ = true;
    checkMute();
}

void Beat::tick(int globalTick, std::vector<BeatEvent>& out) {
    if (updateNotes_) rebuildNotes();
    if (updatePattern_) rebuildPattern();

    const bool effectiveMute = mute_ || externalMute_;
    if (effectiveMute) {
        if (!muted_) {
            muted_ = true;
            BeatEvent ev{index_, noteOff_, 0, false};
            out.push_back(ev);
        }
        return;
    }
    muted_ = false;

    if (offTick_ != 0 && globalTick >= offTick_) {
        BeatEvent ev{index_, noteOff_, 0, false};
        out.push_back(ev);
        offTick_ = 0;
    }

    tickCountdown_ -= 1;
    if (tickCountdown_ > 0) return;

    // Reset the countdown for the next step.
    tickCountdown_ = stepTicks_;

    if (!truths_.empty() && truths_[static_cast<size_t>(truthIndex_)] == 1) {
        BeatEvent on{index_, noteOn_, static_cast<uint8_t>(params_.loud), true};
        out.push_back(on);
        offTick_ = globalTick + sustainTicks_;
    }

    truthIndex_++;
    if (truthIndex_ >= static_cast<int>(truths_.size())) truthIndex_ = 0;
}

BeatEngine::BeatEngine() {
    for (int i = 0; i < kMaxBeats; ++i) beats_[static_cast<size_t>(i)] = Beat(i);
}

void BeatEngine::selectBeat(int oneBased) {
    if (oneBased < 1 || oneBased > kMaxBeats) return;
    selected_ = oneBased - 1;
}

void BeatEngine::setBeatParam(const char* name, int value) {
    beats_[static_cast<size_t>(selected_)].setParam(name, value);
}

void BeatEngine::setLaneMute(int beatIndex, bool muted) {
    if (beatIndex < 0 || beatIndex >= kMaxBeats) return;
    laneMute_[static_cast<size_t>(beatIndex)] = muted;
}

void BeatEngine::setLaneSolo(int beatIndex, bool solo) {
    if (beatIndex < 0 || beatIndex >= kMaxBeats) return;
    laneSolo_[static_cast<size_t>(beatIndex)] = solo;
    anySolo_ = false;
    for (bool s : laneSolo_) {
        if (s) { anySolo_ = true; break; }
    }
}

void BeatEngine::processTick(int globalTick, std::vector<BeatEvent>& out) {
    if (muted_) return;
    for (int i = 0; i < kMaxBeats; ++i) {
        const bool soloGate = anySolo_ && !laneSolo_[static_cast<size_t>(i)];
        beats_[static_cast<size_t>(i)].setExternalMute(laneMute_[static_cast<size_t>(i)] || soloGate);
        beats_[static_cast<size_t>(i)].tick(globalTick, out);
    }
}

void BeatEngine::purgeAll(std::vector<BeatEvent>& out) {
    for (auto& b : beats_) {
        BeatEvent ev{};
        ev.beatIndex = b.params().noteIndex; // store index for visibility, not critical
        ev.note = noteIndexToMidi(b.params().octave, b.params().noteIndex);
        ev.velocity = 0;
        ev.noteOn = false;
        out.push_back(ev);
    }
}

} // namespace beatvst

