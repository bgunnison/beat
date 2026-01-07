#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace beatvst {

constexpr int kMaxBeats = 8;
constexpr int kMaxLoopLength = 32;
constexpr int kMinOctave = -1;
constexpr int kMaxOctave = 9;

struct BeatEvent {
    int beatIndex{};
    uint8_t note{};
    uint8_t velocity{};
    bool noteOn{};
};

struct BeatParams {
    int bars{4};
    int loop{16};
    int beats{4};
    int rotate{0};
    int octave{2};
    int noteIndex{0};
    int loud{0};
};

class Beat {
public:
    explicit Beat(int index = 0);
    bool setParam(const char* name, int value);
    void setParams(const BeatParams& p);
    void setExternalMute(bool muted) { externalMute_ = muted; }
    void tick(int globalTick, std::vector<BeatEvent>& out);
    BeatParams params() const { return params_; }

private:
    int index_{};
    BeatParams params_{};
    std::vector<int> truths_;
    int truthIndex_{0};
    int tickCountdown_{0};
    int stepTicks_{1};
    int sustainTicks_{6};
    int offTick_{0};
    bool mute_{false};
    bool muted_{false};
    bool externalMute_{false};
    bool updatePattern_{true};
    bool updateNotes_{true};
    uint8_t noteOn_{60};  // default middle C
    uint8_t noteOff_{60};

    void rebuildPattern();
    void rebuildNotes();
    void checkMute();
};

class BeatEngine {
public:
    BeatEngine();
    void selectBeat(int oneBased);
    int selectedBeat() const { return selected_ + 1; }
    void setBeatParam(const char* name, int value);
    void setLaneMute(int beatIndex, bool muted);
    void setLaneSolo(int beatIndex, bool solo);
    void setMuted(bool muted) { muted_ = muted; }
    const BeatParams& getBeatParams(int idx) const { return beats_[idx].params(); }
    void processTick(int globalTick, std::vector<BeatEvent>& out);
    void purgeAll(std::vector<BeatEvent>& out);

private:
    std::array<Beat, kMaxBeats> beats_{};
    int selected_{0};
    bool muted_{false};
    std::array<bool, kMaxBeats> laneMute_{};
    std::array<bool, kMaxBeats> laneSolo_{};
    bool anySolo_{false};
};

uint8_t noteIndexToMidi(int octave, int noteIndex);

} // namespace beatvst
