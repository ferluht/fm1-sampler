#include "Stretch.h"
#include <algorithm>
#include <cmath>

namespace {
// time stretch with constant pitch; `speed` > 1 shortens
std::vector<float> wsola(const std::vector<float>& in, int sr, double speed) {
    const int frames = (int)(in.size() / 2);
    const int N = std::max(256, (int)(sr * 0.046)) & ~1, H = N / 2, tol = N / 4;
    const int outFrames = std::max(N, (int)(frames / speed));
    std::vector<float> out((size_t)(outFrames + 2 * N) * 2, 0.f), norm((size_t)(outFrames + 2 * N), 0.f);
    std::vector<float> win((size_t)N); for (int i = 0; i < N; i++) win[(size_t)i] = 0.5f - 0.5f * std::cos(2.f * 3.14159265f * (i + 0.5f) / N);
    auto at = [&](int i, int c) -> float { return i < 0 || i >= frames ? 0.f : in[(size_t)i * 2 + (size_t)c]; };
    auto mono = [&](int i) -> float { return i < 0 || i >= frames ? 0.f : 0.5f * (in[(size_t)i * 2] + in[(size_t)i * 2 + 1]); };
    int prevPos = 0;   // where the last grain was taken from
    for (int k = 0; (int64_t)k * H < outFrames; k++) {
        const int nominal = (int)std::lround(k * H * speed);
        int pos = nominal;
        if (k > 0) {
            // the natural continuation of the previous grain is the best match for this one
            const int ref = prevPos + H;
            double best = -1e30; int bestOff = 0;
            for (int off = -tol; off <= tol; off += 2) {
                const int cand = nominal + off;
                if (cand < 0 || cand >= frames) continue;
                double c = 0, e = 1e-9;
                for (int i = 0; i < N; i += 4) { const float a = mono(ref + i), b = mono(cand + i); c += (double)a * b; e += (double)b * b; }
                const double score = c / std::sqrt(e);
                if (score > best) { best = score; bestOff = off; }
            }
            pos = nominal + bestOff;
        }
        const int o = k * H;
        for (int i = 0; i < N; i++) {
            const float w = win[(size_t)i];
            out[(size_t)(o + i) * 2] += at(pos + i, 0) * w; out[(size_t)(o + i) * 2 + 1] += at(pos + i, 1) * w;
            norm[(size_t)(o + i)] += w;
        }
        prevPos = pos;
    }
    out.resize((size_t)outFrames * 2);
    for (int i = 0; i < outFrames; i++) { const float n = std::max(norm[(size_t)i], 0.25f); out[(size_t)i * 2] /= n; out[(size_t)i * 2 + 1] /= n; }
    return out;
}

// reads the input at `ratio` frames per output frame (4-point Hermite)
std::vector<float> resample(const std::vector<float>& in, double ratio) {
    const int frames = (int)(in.size() / 2), outFrames = std::max(1, (int)(frames / ratio));
    std::vector<float> out((size_t)outFrames * 2);
    auto at = [&](int i, int c) -> float { i = std::min(frames - 1, std::max(0, i)); return in[(size_t)i * 2 + (size_t)c]; };
    for (int i = 0; i < outFrames; i++) {
        const double p = i * ratio; const int i0 = (int)p; const float f = (float)(p - i0);
        for (int c = 0; c < 2; c++) {
            const float y0 = at(i0 - 1, c), y1 = at(i0, c), y2 = at(i0 + 1, c), y3 = at(i0 + 2, c);
            const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            out[(size_t)i * 2 + (size_t)c] = ((c3 * f + c2) * f + c1) * f + c0;
        }
    }
    return out;
}
}  // namespace

std::vector<float> stretchPitch(const std::vector<float>& stereo, int sr, double speed, double semitones) {
    speed = std::min(4.0, std::max(0.25, speed));
    if (std::fabs(speed - 1.0) < 0.005 && std::fabs(semitones) < 0.005) return stereo;
    const double p = std::pow(2.0, semitones / 12.0);
    // resampling by p (reading faster) shortens by p and raises the pitch by p: stretch first by speed / p to land on the wanted duration
    std::vector<float> y = std::fabs(speed / p - 1.0) < 0.005 ? stereo : wsola(stereo, sr, speed / p);
    return std::fabs(p - 1.0) < 0.001 ? y : resample(y, p);
}
