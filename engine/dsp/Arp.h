// Arpeggiator: the held notes are played one by one on a tempo grid. Free-running clock, sample accurate events.
#pragma once
#include "Common.h"
#include <set>
#include <vector>

class Arp {
public:
    struct Ev { int offset, note, vel; };   // vel 0 = note off
    enum Pattern { UP, DOWN, UPDOWN, CONVERGE, RANDOM };

    // rate 0..11: 1, 2, 2T, 4, 4T, 8, 8T, 16, 16T, 32, 32T, 64 (note values; T = triplet)
    void setup(int rate, int pattern, int octaves, float gate) {
        static const float beats[12] = {4.f, 2.f, 4.f / 3.f, 1.f, 2.f / 3.f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f, 1.f / 12.f, 0.0625f};
        beats_ = beats[std::min(11, std::max(0, rate))]; pattern_ = pattern; octaves_ = std::min(4, std::max(1, octaves)); gate_ = clampf(gate, 0.05f, 0.95f);
    }
    void noteOn(int note) { if (held_.empty()) { clock_ = 1e9; step_ = 0; } held_.insert(note); order_.push_back(note); }   // the first note starts at once
    void noteOff(int note) { held_.erase(note); order_.erase(std::remove(order_.begin(), order_.end(), note), order_.end()); }
    void clear(std::vector<Ev>& out) { held_.clear(); order_.clear(); if (playing_ >= 0) { out.push_back({0, playing_, 0}); playing_ = -1; } }

    // advance `n` samples; events are appended with their offset in the block
    void run(int n, double bpm, int sr, std::vector<Ev>& out) {
        const double stepLen = beats_ * 60.0 / bpm * sr;
        for (int i = 0; i < n; i++) {
            if (playing_ >= 0 && gateLeft_-- <= 0) { out.push_back({i, playing_, 0}); playing_ = -1; }
            if (clock_ >= stepLen) {
                clock_ -= stepLen; if (clock_ >= stepLen) clock_ = 0;
                if (playing_ >= 0) { out.push_back({i, playing_, 0}); playing_ = -1; }
                if (!held_.empty()) {
                    playing_ = nextNote();
                    out.push_back({i, playing_, 100});
                    gateLeft_ = (int)(stepLen * gate_);
                }
            }
            clock_ += 1.0;
        }
        if (held_.empty() && playing_ >= 0) { out.push_back({n > 0 ? n - 1 : 0, playing_, 0}); playing_ = -1; }
    }
private:
    int nextNote() {
        std::vector<int> seq;
        for (int o = 0; o < octaves_; o++) for (int n : held_) seq.push_back(n + 12 * o);   // ascending (set order) per octave
        const int N = (int)seq.size();
        int idx = 0;
        switch (pattern_) {
            case UP: idx = step_ % N; break;
            case DOWN: idx = N - 1 - (step_ % N); break;
            case UPDOWN: { const int period = std::max(1, 2 * N - 2), k = step_ % period; idx = k < N ? k : period - k; break; }
            case CONVERGE: { const int k = step_ % N; idx = (k % 2 == 0) ? k / 2 : N - 1 - k / 2; break; }
            default: rng_ = rng_ * 1664525u + 1013904223u; idx = (int)((rng_ >> 8) % (uint32_t)N); break;
        }
        step_++;
        return std::min(127, seq[(size_t)idx]);
    }
    float beats_ = 0.25f, gate_ = 0.7f; int pattern_ = 0, octaves_ = 1, step_ = 0, playing_ = -1, gateLeft_ = 0;
    double clock_ = 0; uint32_t rng_ = 99;
    std::set<int> held_; std::vector<int> order_;
};
