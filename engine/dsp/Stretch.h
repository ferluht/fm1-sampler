// Offline time stretch and pitch shift of a stereo buffer: WSOLA (waveform similarity overlap-add) for the duration, a resampler for the pitch.
#pragma once
#include <vector>

// speed > 1 plays faster (shorter), semitones shifts the pitch without changing the duration. Interleaved stereo in and out.
std::vector<float> stretchPitch(const std::vector<float>& stereo, int sampleRate, double speed, double semitones);
