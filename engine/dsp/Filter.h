// Two-pole low-pass (state variable filter, trapezoidal integration), stereo.
#pragma once
#include "Common.h"

struct Lowpass {
    float a1 = 0, a2 = 0, a3 = 0, ic1[2] = {0, 0}, ic2[2] = {0, 0};
    void set(float hz, float sr, float q = 0.8f) {
        const float g = std::tan(kPi * clampf(hz, 20.f, sr * 0.45f) / sr), k = 1.f / q;
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    float process(int ch, float x) {
        const float v3 = x - ic2[ch], v1 = a1 * ic1[ch] + a2 * v3, v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.f * v1 - ic1[ch]; ic2[ch] = 2.f * v2 - ic2[ch];
        return v2;
    }
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f; }
};

// One-pole low-pass for tone and feedback damping.
struct OnePole {
    float a = 0.5f, z[2] = {0, 0};
    void set(float hz, float sr) { a = 1.f - std::exp(-2.f * kPi * hz / sr); }
    float process(int ch, float x) { z[ch] += a * (x - z[ch]); return z[ch]; }
};
