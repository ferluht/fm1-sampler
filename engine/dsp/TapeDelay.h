// Stereo delay with a tape flavour: the time glides (pitch slides when it moves), the feedback is darkened and softly saturated, a slow wow moves the time.
// Wet signal only.
#pragma once
#include "Common.h"
#include "Filter.h"
#include <vector>

class TapeDelay {
public:
    explicit TapeDelay(int sr) : sr_(sr), len_((size_t)(sr * 3.3f)) {
        for (auto& b : buf_) b.assign(len_, 0.f);
        damp_.set(4500.f, (float)sr);
    }
    void setTime(float sec) { target_ = clampf(sec, 0.02f, 3.0f) * sr_; }
    void setFeedback(float fb) { fb_ = clampf(fb, 0.f, 0.95f); }
    void process(const float* in, float* out, int n) {
        for (int i = 0; i < n; i++) {
            time_ += (target_ - time_) * 0.0004f;   // glide
            wowPh_ += 0.55f / sr_; if (wowPh_ >= 1.f) wowPh_ -= 1.f;
            const float t = time_ + std::sin(2.f * kPi * wowPh_) * 0.0003f * sr_;
            double rp = (double)wr_ - t; while (rp < 0) rp += (double)len_;
            const size_t i0 = (size_t)rp % len_, i1 = (i0 + 1) % len_; const float fr = (float)(rp - std::floor(rp));
            for (int c = 0; c < 2; c++) {
                const float d = buf_[c][i0] + (buf_[c][i1] - buf_[c][i0]) * fr;
                out[2 * i + c] = d;
                const float fbIn = std::tanh(damp_.process(c, d) * fb_ * 1.2f) / 1.2f;
                buf_[c][wr_] = in[2 * i + c] + fbIn;
            }
            if (++wr_ >= len_) wr_ = 0;
        }
    }
private:
    int sr_; size_t len_, wr_ = 0;
    std::vector<float> buf_[2];
    float time_ = 0.4f * 48000, target_ = 0.4f * 48000, fb_ = 0.4f, wowPh_ = 0.f;
    OnePole damp_;
};
