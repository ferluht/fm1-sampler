#include "Wav.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace {
uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
}  // namespace

bool readWav(const std::string& path, std::vector<float>& stereo, int& sampleRate) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < 44 || memcmp(b.data(), "RIFF", 4) || memcmp(b.data() + 8, "WAVE", 4)) return false;
    int fmt = 0, ch = 0, bits = 0; sampleRate = 0;
    const uint8_t* data = nullptr; size_t dataLen = 0;
    for (size_t p = 12; p + 8 <= b.size();) {
        const uint32_t len = rd32(&b[p + 4]);
        if (!memcmp(&b[p], "fmt ", 4) && p + 8 + 16 <= b.size()) {
            fmt = rd16(&b[p + 8]); ch = rd16(&b[p + 10]); sampleRate = (int)rd32(&b[p + 12]); bits = rd16(&b[p + 22]);
            if (fmt == 0xFFFE && len >= 26 && p + 8 + 26 <= b.size()) fmt = rd16(&b[p + 8 + 24]);   // extensible: the sub-format
        } else if (!memcmp(&b[p], "data", 4)) {
            data = &b[p + 8]; dataLen = std::min<size_t>(len, b.size() - p - 8); break;
        }
        p += 8 + (size_t)len + (len & 1);
    }
    if (!data || ch < 1 || sampleRate < 8000 || (fmt != 1 && fmt != 3)) return false;
    const int bps = bits / 8;
    if (bps < 1 || bps > 4 || (fmt == 3 && bps != 4)) return false;
    const size_t frames = dataLen / ((size_t)bps * (size_t)ch);
    stereo.assign(frames * 2, 0.f);
    auto sample = [&](size_t frame, int c) -> float {
        const uint8_t* p = data + (frame * (size_t)ch + (size_t)c) * (size_t)bps;
        if (fmt == 3) { float v; memcpy(&v, p, 4); return v; }
        switch (bps) {
            case 1: return ((int)p[0] - 128) / 128.f;
            case 2: return (int16_t)rd16(p) / 32768.f;
            case 3: { int32_t v = (int32_t)(p[0] << 8 | p[1] << 16 | (uint32_t)p[2] << 24); return v / 2147483648.f; }
            default: return (int32_t)rd32(p) / 2147483648.f;
        }
    };
    for (size_t i = 0; i < frames; i++) { stereo[2 * i] = sample(i, 0); stereo[2 * i + 1] = ch > 1 ? sample(i, 1) : stereo[2 * i]; }
    return frames > 0;
}

bool writeWav16(const std::string& path, const std::vector<float>& st, int sr) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint32_t n = (uint32_t)st.size() * 2, br = (uint32_t)sr * 4, riff = 36 + n, sixteen = 16, rate = (uint32_t)sr;
    const uint16_t fmt = 1, ch = 2, ba = 4, bits = 16;
    f.write("RIFF", 4); f.write((const char*)&riff, 4); f.write("WAVEfmt ", 8); f.write((const char*)&sixteen, 4);
    f.write((const char*)&fmt, 2); f.write((const char*)&ch, 2); f.write((const char*)&rate, 4); f.write((const char*)&br, 4);
    f.write((const char*)&ba, 2); f.write((const char*)&bits, 2); f.write("data", 4); f.write((const char*)&n, 4);
    for (float v : st) { const int16_t q = (int16_t)std::lrintf(std::max(-1.f, std::min(1.f, v)) * 32767.f); f.write((const char*)&q, 2); }
    return (bool)f;
}
