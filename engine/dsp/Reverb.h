// Stereo reverb: eight damped feedback combs per side in parallel, four allpasses in series (a Schroeder / Freeverb style network).
// Wet signal only: the mixer scales the return.
#pragma once
#include "Common.h"
#include <vector>

class Reverb {
public:
    explicit Reverb(int sr) : sr_(sr) {
        static const int combLen[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int apLen[4] = {556, 441, 341, 225};
        const float k = sr / 44100.f;
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i < 8; i++) comb_[s][i].init((int)((combLen[i] + 23 * s) * k));
            for (int i = 0; i < 4; i++) ap_[s][i].init((int)((apLen[i] + 11 * s) * k));
        }
        setSize(0.7f);
    }
    // size 0..1: the length of the tail
    void setSize(float size) { feedback_ = 0.70f + 0.28f * clampf(size, 0.f, 1.f); }
    void process(const float* in /*stereo*/, float* out /*stereo, overwritten*/, int n) {
        for (int i = 0; i < n; i++) {
            const float x = (in[2 * i] + in[2 * i + 1]) * 0.015f;
            for (int s = 0; s < 2; s++) {
                float y = 0.f;
                for (auto& c : comb_[s]) y += c.process(x, feedback_, damp_);
                for (auto& a : ap_[s]) y = a.process(y);
                out[2 * i + s] = y;
            }
        }
    }
private:
    struct Comb {
        std::vector<float> buf; int pos = 0; float lp = 0.f;
        void init(int n) { buf.assign((size_t)std::max(1, n), 0.f); }
        float process(float x, float fb, float damp) {
            const float y = buf[(size_t)pos];
            lp = y * (1.f - damp) + lp * damp;
            buf[(size_t)pos] = x + lp * fb;
            if (++pos >= (int)buf.size()) pos = 0;
            return y;
        }
    };
    struct Allpass {
        std::vector<float> buf; int pos = 0;
        void init(int n) { buf.assign((size_t)std::max(1, n), 0.f); }
        float process(float x) {
            const float b = buf[(size_t)pos], y = b - x;
            buf[(size_t)pos] = x + b * 0.5f;
            if (++pos >= (int)buf.size()) pos = 0;
            return y;
        }
    };
    int sr_; float feedback_ = 0.9f, damp_ = 0.3f;
    Comb comb_[2][8]; Allpass ap_[2][4];
};
