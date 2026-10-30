// Extreme time stretch ("Paulstretch"): overlapping windows of the source are turned into spectra, the phases are randomised, and the
// result is overlap-added, so the sound is smeared into an endless pad. One stream per voice.
#pragma once
#include <memory>
#include <vector>

class PaulStream {
public:
    PaulStream();
    ~PaulStream();
    // the stereo source (interleaved, `frames` long, must outlive the stream) and the window [from, to) in frames
    void start(const float* src, int frames, double from, double to, float stretch);
    // one stereo frame at `rate` frames of the stream per call (the stream is generated on demand)
    void next(double rate, float& l, float& r);
    double sourcePos() const { return srcPos_; }
private:
    void makeHop();
    struct Fft; std::unique_ptr<Fft> fft_;
    const float* src_ = nullptr; int frames_ = 0; double from_ = 0, to_ = 0, srcPos_ = 0; float stretch_ = 8.f;
    std::vector<float> fifo_, ola_, win_;
    size_t fifoW_ = 0; double readPos_ = 0; size_t avail_ = 0; uint32_t rng_ = 1;
    static constexpr int N = 4096, HOP = N / 2;   // 4096: a hop (two FFTs per channel) stays cheap enough for a phone's audio thread
};
