// Envelope: linear attack, exponential decay and release.
#pragma once
#include "Common.h"

struct Adsr {
    enum Stage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    int stage = IDLE;
    float level = 0.f, sustain = 0.8f, atkInc = 0.01f, decCoef = 0.001f, relCoef = 0.999f;

    // times in seconds; the minimum times keep note edges click free
    void set(float attack, float decay, float sus, float release, float sr) {
        atkInc = 1.f / (std::max(attack, 0.002f) * sr);
        decCoef = 1.f - std::exp(-4.6f / (std::max(decay, 0.005f) * sr));
        relCoef = std::exp(-4.6f / (std::max(release, 0.01f) * sr));
        sustain = clampf(sus, 0.f, 1.f);
    }
    void gateOn() { stage = ATTACK; level = 0.f; }
    void gateOff() { if (stage != IDLE) stage = RELEASE; }
    bool done() const { return stage == IDLE; }
    float next() {
        switch (stage) {
            case ATTACK: level += atkInc; if (level >= 1.f) { level = 1.f; stage = DECAY; } break;
            case DECAY: level += (sustain - level) * decCoef; if (std::fabs(level - sustain) < 1e-4f) { level = sustain; stage = SUSTAIN; } break;
            case RELEASE: level *= relCoef; if (level < 1e-4f) { level = 0.f; stage = IDLE; } break;
            default: break;
        }
        return level;
    }
};
