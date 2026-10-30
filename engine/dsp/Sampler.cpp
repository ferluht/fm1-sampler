#include "Sampler.h"
#include "Slicer.h"
#include "Wav.h"
#include <cstring>

Sampler::Sampler(int engineRate) : sr_(engineRate) {
    for (int i = 0; i < kVoices; i++) paul_.emplace_back(new PaulStream);
}
Sampler::~Sampler() = default;

bool Sampler::load(const std::string& path) {
    auto buf = std::make_shared<SampleBuf>();
    if (!readWav(path, buf->d, buf->sr)) return false;
    buf->frames = (int)(buf->d.size() / 2);
    if (buf->frames < 64) return false;
    float pk = 0.f; for (float v : buf->d) pk = std::max(pk, std::fabs(v));
    if (pk > 1e-4f) { const float g = std::min(8.f, 0.9f / pk); for (float& v : buf->d) v *= g; }   // quiet files are brought up to a usable level
    peaks_.assign(512, 0.f);
    for (int b = 0; b < 512; b++) {
        const long a = (long)buf->frames * b / 512, e = std::max(a + 1, (long)buf->frames * (b + 1) / 512), step = std::max(1L, (e - a) / 64);
        float m = 0.f; for (long i = a; i < e; i += step) m = std::max(m, std::fabs(buf->d[(size_t)i * 2]));
        peaks_[(size_t)b] = m;
    }
    slices_ = findSlices(buf->d, buf->frames, buf->sr, 16);
    const size_t slash = path.find_last_of("/\\"), dot = path.rfind('.');
    label_ = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (dot != std::string::npos && dot > (slash == std::string::npos ? 0 : slash)) label_ = label_.substr(0, label_.rfind('.'));
    orig_ = buf;
    return true;
}

void Sampler::setMode(int m) { m = std::min(kModes - 1, std::max(0, m)); if (m != mode_) { allOff(); mode_ = m; } }
void Sampler::setEnv(float a, float d, float s, float r) { atk_ = a; dec_ = d; sus_ = s; rel_ = r; }
void Sampler::setRange(float a, float b) { rangeA_ = clampf(a, 0.f, 1.f); rangeB_ = clampf(b, 0.f, 1.f); if (rangeB_ < rangeA_ + 0.005f) rangeB_ = std::min(1.f, rangeA_ + 0.005f); }
void Sampler::setProcessed(std::shared_ptr<const SampleBuf> p, float a, float b) { allOff(); proc_ = std::move(p); procA_ = a; procB_ = std::max(a + 0.001f, b); }

void Sampler::resolveWindow(Voice& v, const SampleBuf*& buf, double& ws, double& we) const {
    if (mode_ == KIT || !proc_) {
        buf = orig_.get();
        if (mode_ == KIT) {
            const int n = (int)slices_.size(), i = v.slice % n;
            ws = slices_[(size_t)i]; we = i + 1 < n ? slices_[(size_t)i + 1] : buf->frames;
        } else { ws = rangeA_ * buf->frames; we = rangeB_ * buf->frames; }
    } else {
        buf = proc_.get();   // a stretched copy of [procA, procB]: the window is the same range inside it
        const double span = procB_ - procA_;
        ws = clampf((float)((rangeA_ - procA_) / span), 0.f, 1.f) * buf->frames; we = clampf((float)((rangeB_ - procA_) / span), 0.f, 1.f) * buf->frames;
    }
    we = std::min<double>(we, buf->frames - 2); ws = std::min(ws, we - 16);
    if (ws < 0) ws = 0;
}

void Sampler::noteOn(int note, int vel) {
    if (!orig_) return;
    int vi = -1; uint32_t oldest = 0xffffffffu;
    if (mode_ == PAUL) {   // each voice costs FFTs: at most four at once
        int active = 0; for (auto& v : voices_) active += v.on;
        if (active >= 4) { for (int i = 0; i < kVoices; i++) if (voices_[i].on && voices_[i].born < oldest) { oldest = voices_[i].born; vi = i; } }
    }
    if (vi < 0) for (int i = 0; i < kVoices; i++) if (!voices_[i].on) { vi = i; break; }
    if (vi < 0) { oldest = 0xffffffffu; for (int i = 0; i < kVoices; i++) if (voices_[i].born < oldest) { oldest = voices_[i].born; vi = i; } }
    Voice& v = voices_[vi];
    v = Voice();
    v.on = true; v.gate = true; v.note = note; v.vel = clampf(vel / 127.f, 0.f, 1.f); v.born = ++bornCounter_;
    v.slice = ((note - 60) % 16 + 16) % 16;
    resolveWindow(v, v.buf, v.ws, v.we);
    v.pos = v.ws;
    v.env.set(atk_, dec_, sus_, rel_, (float)sr_);
    if (mode_ == ONESHOT) v.env.set(0.002f, 0.01f, 1.f, 0.01f, (float)sr_);
    v.env.gateOn();
    if (mode_ == PAUL) { v.paulIdx = vi; paul_[(size_t)vi]->start(v.buf->d.data(), v.buf->frames, v.ws, v.we, 8.f); }
}
void Sampler::noteOff(int note) {
    if (mode_ == ONESHOT) return;   // always plays out
    for (auto& v : voices_) if (v.on && v.gate && v.note == note) { v.gate = false; v.env.gateOff(); }
}
void Sampler::allOff() { for (auto& v : voices_) v = Voice(); }
bool Sampler::hasVoices() const { for (auto& v : voices_) if (v.on) return true; return false; }

void Sampler::render(float* out, int n) {
    if (!orig_) return;
    for (auto& v : voices_) if (v.on) renderVoice(v, out, n);
}

void Sampler::renderVoice(Voice& v, float* out, int n) {
    const float* d = v.buf->d.data(); const int frames = v.buf->frames;
    const double srcRate = (double)v.buf->sr / sr_;
    const double pitch = mode_ == KIT ? 1.0 : std::pow(2.0, (v.note - 60 + bend_) / 12.0);
    const double rate = srcRate * pitch;
    auto readAt = [&](double p, float& l, float& r) {
        const int i0 = std::min(frames - 2, std::max(0, (int)p)); const float f = (float)(p - i0);
        l = d[(size_t)i0 * 2] + (d[(size_t)i0 * 2 + 2] - d[(size_t)i0 * 2]) * f;
        r = d[(size_t)i0 * 2 + 1] + (d[(size_t)i0 * 2 + 3] - d[(size_t)i0 * 2 + 1]) * f;
    };
    const float gain = 0.8f * v.vel;
    for (int i = 0; i < n; i++) {
        float l = 0.f, r = 0.f;
        const float e = v.env.next();
        if (v.env.done() && !v.gate) { v.on = false; return; }
        if (mode_ == PAUL) {
            PaulStream& ps = *paul_[(size_t)v.paulIdx];
            ps.next(pitch, l, r);
            v.pos = ps.sourcePos();
        } else if (mode_ == GRAIN) {
            v.spawn -= 1.0;
            if (v.spawn <= 0.0) {
                for (auto& g : v.grain) if (!g.on) {
                    g.on = true; g.age = 0; g.len = (int)(sr_ * (0.06f + 0.12f * rnd()));
                    const float k = rnd(); const double mult = k < 0.15 ? 0.5 : k < 0.3 ? 2.0 : 1.0;
                    g.inc = rate * mult * (1.0 + (rnd() - 0.5) * 0.004) * (rnd() < 0.2f ? -1.0 : 1.0);
                    const double span = std::max(1.0, v.we - v.ws - g.len * std::fabs(g.inc));
                    g.pos = g.inc > 0 ? v.ws + rnd() * span : v.we - rnd() * span;
                    break;
                }
                v.spawn = sr_ * 0.018 * (0.6 + 0.8 * rnd());
            }
            for (auto& g : v.grain) {
                if (!g.on) continue;
                if (g.age >= g.len || g.pos < 1 || g.pos >= frames - 2) { g.on = false; continue; }
                const float w = 0.5f - 0.5f * std::cos(2.f * kPi * (g.age + 0.5f) / g.len);
                float gl, gr; readAt(g.pos, gl, gr); l += gl * w; r += gr * w;
                g.pos += g.inc; g.age++;
            }
            l *= 0.5f; r *= 0.5f;
        } else {
            double p = v.pos;
            if (mode_ == LOOPED) {
                const double xf = std::min(256.0, (v.we - v.ws) * 0.25);
                readAt(p, l, r);
                if (p >= v.we - xf) { const double t = (p - (v.we - xf)) / xf; float l2, r2; readAt(v.ws + (p - (v.we - xf)), l2, r2); l = l * (float)(1 - t) + l2 * (float)t; r = r * (float)(1 - t) + r2 * (float)t; }
                v.pos += rate;
                if (v.pos >= v.we) v.pos = v.ws + xf + (v.pos - v.we);
            } else {
                if (p >= v.we) { v.on = false; return; }
                readAt(p, l, r);
                const float tail = (float)std::min(1.0, (v.we - p) / (128.0 * rate));   // short fade at the end of the window
                l *= tail; r *= tail;
                v.pos += rate;
            }
        }
        out[2 * i] += l * e * gain; out[2 * i + 1] += r * e * gain;
    }
}

float Sampler::orginalPosOf(const Voice& v) const {
    if (!orig_) return 0.f;
    double pos = v.pos;
    if (v.buf == proc_.get() && proc_) pos = (procA_ + pos / std::max(1, v.buf->frames) * (procB_ - procA_)) * orig_->frames;
    else if (v.buf && v.buf->frames != orig_->frames) pos = pos / v.buf->frames * orig_->frames;
    return clampf((float)(pos / orig_->frames), 0.f, 1.f);
}

int Sampler::voicePositions(float* out, int maxN) const {
    int n = 0;
    for (auto& v : voices_) {
        if (!v.on || n >= maxN) continue;
        if (mode_ == GRAIN) {
            for (auto& g : v.grain) if (g.on && n < maxN) {
                Voice tmp = v; tmp.pos = g.pos; out[n++] = orginalPosOf(tmp);
            }
        } else out[n++] = orginalPosOf(v);
    }
    return n;
}
