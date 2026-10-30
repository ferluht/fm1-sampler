// Finds drum hits in a sample: the start frames of up to `count` slices, sorted. Equal slices when the sample has no clear onsets.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

inline std::vector<int> findSlices(const std::vector<float>& stereo, int frames, int sr, int count = 16) {
    std::vector<int> starts;
    const int hop = std::max(64, sr / 200), bins = frames / hop;
    if (bins >= 8) {
        std::vector<float> e((size_t)bins, 0.f);
        float peak = 1e-6f;
        for (int b = 0; b < bins; b++) {
            float s = 0.f; for (int i = b * hop; i < (b + 1) * hop; i++) s += std::fabs(stereo[(size_t)i * 2]) + std::fabs(stereo[(size_t)i * 2 + 1]);
            e[(size_t)b] = s / (2.f * hop); peak = std::max(peak, e[(size_t)b]);
        }
        std::vector<std::pair<float, int>> cand;   // onset strength, bin
        for (int b = 2; b < bins - 1; b++) {
            float avg = 0.f; int k = 0; for (int j = std::max(0, b - 12); j < b - 1; j++) { avg += e[(size_t)j]; k++; }
            avg = avg / std::max(1, k) + 0.01f * peak;
            const float ratio = e[(size_t)b] / avg;
            if (ratio > 2.5f && e[(size_t)b] > 0.05f * peak && e[(size_t)b] >= e[(size_t)b - 1] && e[(size_t)b] >= e[(size_t)b + 1]) cand.push_back({ratio * e[(size_t)b], b});
        }
        std::sort(cand.begin(), cand.end(), [](auto& a, auto& b) { return a.first > b.first; });
        const int minGap = std::max(1, sr / 15 / hop);   // 65 ms
        for (auto& c : cand) {
            if ((int)starts.size() >= count) break;
            bool near = false; for (int s : starts) if (std::abs(s / hop - c.second) < minGap) near = true;
            if (!near) starts.push_back(std::max(0, (c.second - 1) * hop));
        }
        std::sort(starts.begin(), starts.end());
        if (starts.size() >= 2 && starts[0] > hop * 4) starts.insert(starts.begin(), 0);
    }
    if (starts.size() < 2) { starts.clear(); for (int i = 0; i < count; i++) starts.push_back((int)((int64_t)frames * i / count)); }
    return starts;
}
