// FM1-DAW design system. One palette, one set of radii and spacings; the FM-1 screen (surface/Surface.cpp) uses the same hues.
// Colours are ARGB. Sizes are dp.
#pragma once
#include <cmath>
#include <cstdint>

namespace theme {
constexpr uint32_t argb(int r, int g, int b, int a = 255) { return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
constexpr uint32_t withAlpha(uint32_t c, int a) { return (c & 0x00ffffffu) | (uint32_t)a << 24; }

// Lo-fi look: two base colours (paper and ink, in a dark and a light variant) and one accent. No gradients, 1 px lines, square corners.
// The colours are variables so the phone can switch variant at run time (theme::apply); everything else names them through this header.
inline uint32_t BG, SURFACE, RAISED, OUTLINE, TEXT, DIM, ACCENT, ON_ACCENT;
inline uint32_t VIOLET, GREEN, RED, AMBER;   // old names kept: they now all resolve to paper / accent, so state is told by fill and position, not hue
inline bool light = false;
inline void apply(bool lightVariant) {
    light = lightVariant;
    const uint32_t paper = argb(237, 235, 228), ink = argb(20, 20, 26);
    ACCENT = argb(232, 163, 61);                       // amber: playhead, selection, recording
    if (!light) { BG = ink; SURFACE = argb(28, 28, 36); RAISED = argb(40, 40, 50); OUTLINE = argb(66, 66, 78); TEXT = paper; DIM = argb(138, 136, 128); }
    else { BG = paper; SURFACE = argb(226, 223, 214); RAISED = argb(212, 209, 198); OUTLINE = argb(168, 165, 154); TEXT = ink; DIM = argb(110, 108, 100); }
    ON_ACCENT = ink;
    VIOLET = TEXT; GREEN = TEXT; RED = ACCENT; AMBER = ACCENT;
}
inline const bool kInit = (apply(false), true);

constexpr float R_CARD = 2, R_CONTROL = 2, GAP = 6, MARGIN = 8;
constexpr float T_CAPTION = 10, T_BODY = 12, T_VALUE = 15, T_TITLE = 18, T_NUMBER = 22;

// Tracks are told apart by number and position, not hue.
inline uint32_t trackColor(int) { return TEXT; }
}  // namespace theme
