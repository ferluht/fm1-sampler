// Small shared helpers of the DSP code.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef SAMPLERATE
#define SAMPLERATE 48000   // default engine rate on the host; the Android build defines 44100 (the FM-1's USB audio rate)
#endif

constexpr float kPi = 3.14159265358979f;
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

// Flush-to-zero / denormals-are-zero for the calling thread: decaying tails (reverb, filters) must not slow the CPU down.
namespace Denormals {
inline void enableOnThisThread() {
#if defined(__SSE__)
    unsigned int csr; asm volatile("stmxcsr %0" : "=m"(csr)); csr |= 0x8040u; asm volatile("ldmxcsr %0" : : "m"(csr));
#elif defined(__aarch64__)
    uint64_t fpcr; asm volatile("mrs %0, fpcr" : "=r"(fpcr)); fpcr |= (1ull << 24); asm volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
}
}  // namespace Denormals
