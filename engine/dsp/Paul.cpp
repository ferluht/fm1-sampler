#include "Paul.h"
#include <cmath>
#include <cstdint>
#include <cstring>
extern "C" {
#include "kiss_fftr.h"
}

struct PaulStream::Fft {
    kiss_fftr_cfg fwd = nullptr, inv = nullptr;
    Fft() { fwd = kiss_fftr_alloc(PaulStream::N, 0, nullptr, nullptr); inv = kiss_fftr_alloc(PaulStream::N, 1, nullptr, nullptr); }
    ~Fft() { free(fwd); free(inv); }
};

PaulStream::PaulStream() : fft_(new Fft), fifo_((size_t)N * 16 * 2, 0.f), ola_((size_t)HOP * 2, 0.f), win_((size_t)N) {
    for (int i = 0; i < N; i++) win_[(size_t)i] = std::sin(3.14159265f * (i + 0.5f) / N);   // sine window: its square sums to 1 at 50 % overlap
}
PaulStream::~PaulStream() = default;

void PaulStream::start(const float* src, int frames, double from, double to, float stretch) {
    src_ = src; frames_ = frames; from_ = std::max(0.0, from); to_ = std::min((double)frames, to); stretch_ = stretch;
    if (to_ - from_ < N) { from_ = std::max(0.0, std::min(from_, (double)frames - N)); to_ = std::min((double)frames, from_ + N); }
    srcPos_ = from_; std::fill(fifo_.begin(), fifo_.end(), 0.f); std::fill(ola_.begin(), ola_.end(), 0.f);
    fifoW_ = 0; readPos_ = 0; avail_ = 0; rng_ = rng_ * 747796405u + 2891336453u;
}

void PaulStream::makeHop() {
    const size_t cap = fifo_.size() / 2;
    float tin[N]; kiss_fft_cpx spec[N / 2 + 1];
    float hopOut[2][HOP];
    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < N; i++) {
            double p = srcPos_ + i; while (p >= to_) p -= (to_ - from_);
            const int i0 = std::min(frames_ - 1, (int)p);
            tin[i] = src_[(size_t)i0 * 2 + (size_t)c] * win_[(size_t)i];
        }
        kiss_fftr(fft_->fwd, tin, spec);
        for (int k = 0; k <= N / 2; k++) {
            const float mag = std::sqrt(spec[k].r * spec[k].r + spec[k].i * spec[k].i);
            rng_ = rng_ * 1664525u + 1013904223u;
            const float ph = (rng_ >> 8) * (6.2831853f / 16777216.f);
            spec[k].r = mag * std::cos(ph); spec[k].i = mag * std::sin(ph);
        }
        spec[0].i = 0; spec[N / 2].i = 0;
        kiss_fftri(fft_->inv, spec, tin);
        for (int i = 0; i < N; i++) tin[i] *= win_[(size_t)i] * (1.f / N);
        for (int i = 0; i < HOP; i++) { hopOut[c][i] = tin[i] + ola_[(size_t)c * HOP + (size_t)i]; ola_[(size_t)c * HOP + (size_t)i] = tin[i + HOP]; }
    }
    for (int i = 0; i < HOP; i++) { fifo_[(fifoW_ % cap) * 2] = hopOut[0][i]; fifo_[(fifoW_ % cap) * 2 + 1] = hopOut[1][i]; fifoW_++; }
    avail_ += HOP;
    srcPos_ += HOP / stretch_; while (srcPos_ >= to_) srcPos_ -= (to_ - from_);
}

void PaulStream::next(double rate, float& l, float& r) {
    const size_t cap = fifo_.size() / 2;
    while (avail_ < (size_t)(readPos_ + 4)) makeHop();   // generate on demand
    const size_t i0 = (size_t)readPos_; const float f = (float)(readPos_ - i0);
    const size_t a = (fifoW_ - avail_ + i0) % cap, b = (a + 1) % cap;
    l = fifo_[a * 2] + (fifo_[b * 2] - fifo_[a * 2]) * f; r = fifo_[a * 2 + 1] + (fifo_[b * 2 + 1] - fifo_[a * 2 + 1]) * f;
    readPos_ += rate;
    if (readPos_ >= 2048) { const size_t drop = (size_t)readPos_; avail_ -= drop; readPos_ -= (double)drop; }   // keep the read pointer near the start of the data
}
