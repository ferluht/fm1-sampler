// The sampler instrument: one sample, up to eight voices, six playback algorithms.
//   MELODIC  pitched by the note, ADSR, plays the window once
//   ONESHOT  pitched by the note, always plays the whole window (note off is ignored)
//   LOOPED   pitched by the note, ADSR, the window loops while the key is held
//   KIT      the sample is cut at its hits into 16 slices; a note plays one slice unpitched (pad = note - 60)
//   GRAIN    a cloud of short overlapping grains picked at random inside the window
//   PAUL     the window smeared into an endless pad (spectral time stretch)
#pragma once
#include "Adsr.h"
#include "Common.h"
#include "Paul.h"
#include <memory>
#include <string>
#include <vector>

struct SampleBuf {
    std::vector<float> d;   // interleaved stereo
    int frames = 0, sr = SAMPLERATE;
};

class Sampler {
public:
    enum Mode { MELODIC, ONESHOT, LOOPED, KIT, GRAIN, PAUL, kModes };
    static constexpr int kVoices = 8;

    Sampler(int engineRate);
    ~Sampler();
    // worker thread: reads a WAV, normalises it, finds the slices and the overview peaks
    bool load(const std::string& path);
    const std::string& label() const { return label_; }
    const std::vector<float>& peaks() const { return peaks_; }   // 512 bins, 0..1
    std::shared_ptr<const SampleBuf> original() const { return orig_; }

    // audio thread
    void setMode(int m);
    void setEnv(float attack, float decay, float sustain, float release);
    void setRange(float a, float b);   // 0..1 of the original sample
    // the offline stretched / pitched copy of the range [a, b] (null: play the original)
    void setProcessed(std::shared_ptr<const SampleBuf> p, float a, float b);
    void setBend(float semitones) { bend_ = semitones; }
    void noteOn(int note, int vel);
    void noteOff(int note);
    void allOff();
    void render(float* out /*stereo, added*/, int n);
    int voicePositions(float* out, int maxN) const;   // read positions 0..1 of the original sample
    bool hasVoices() const;

private:
    struct Grain { bool on = false; double pos = 0, inc = 0; int age = 0, len = 1; };
    struct Voice {
        bool on = false, gate = false; int note = 0, slice = 0; float vel = 1.f; uint32_t born = 0;
        double pos = 0, rate = 1.0, ws = 0, we = 0;   // window of the played buffer, frames
        const SampleBuf* buf = nullptr;
        Adsr env; Grain grain[12]; double spawn = 0; int paulIdx = -1;
    };
    void resolveWindow(Voice& v, const SampleBuf*& buf, double& ws, double& we) const;
    float orginalPosOf(const Voice& v) const;
    void renderVoice(Voice& v, float* out, int n);
    float rnd() { rng_ = rng_ * 1664525u + 1013904223u; return (rng_ >> 8) / 16777216.f; }

    int sr_;
    std::shared_ptr<const SampleBuf> orig_, proc_;
    float procA_ = 0, procB_ = 1, rangeA_ = 0, rangeB_ = 1, bend_ = 0, atk_ = 0.01f, dec_ = 0.3f, sus_ = 0.8f, rel_ = 0.3f;
    int mode_ = MELODIC; std::vector<int> slices_; std::string label_; std::vector<float> peaks_;
    Voice voices_[kVoices]; std::vector<std::unique_ptr<PaulStream>> paul_; bool paulBusy_[kVoices] = {};
    uint32_t bornCounter_ = 0, rng_ = 7;
};
