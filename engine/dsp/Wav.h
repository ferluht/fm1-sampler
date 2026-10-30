// Minimal WAV reader / writer: PCM 8/16/24/32 and float32 in, 16-bit stereo out.
#pragma once
#include <string>
#include <vector>

// Reads any mono or multi-channel WAV into interleaved stereo floats (mono is duplicated, extra channels dropped).
bool readWav(const std::string& path, std::vector<float>& stereo, int& sampleRate);
bool writeWav16(const std::string& path, const std::vector<float>& stereo, int sampleRate);
