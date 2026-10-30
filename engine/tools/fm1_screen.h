// Host model of the FM-1 firmware's draw commands (app/ctrl.c): decodes the same SysEx into a 240x240 RGB565 frame.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
struct Fm1Screen {
    uint16_t px[240 * 240] = {};
    uint32_t frames = 0, bytes = 0;
    static const uint8_t* glyph(unsigned c) {
        static const uint8_t font[36][5] = {
            {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0}, {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10},
            {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e},
            {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
            {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0, 0x41, 0x7f, 0x41, 0}, {0x20, 0x40, 0x41, 0x3f, 0x01},
            {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
            {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
            {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f}, {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};
        static const uint8_t punct[][6] = {{'.', 0, 0x60, 0x60, 0, 0}, {':', 0, 0x36, 0x36, 0, 0}, {'-', 8, 8, 8, 8, 8}, {'/', 0x20, 0x10, 0x08, 0x04, 0x02}, {'%', 0x23, 0x13, 0x08, 0x64, 0x62},
            {'+', 8, 8, 0x3e, 8, 8}, {'>', 0, 0x41, 0x22, 0x14, 8}, {'<', 8, 0x14, 0x22, 0x41, 0}, {'_', 0x40, 0x40, 0x40, 0x40, 0x40}, {'!', 0, 0, 0x5f, 0, 0}, {'?', 2, 1, 0x51, 9, 6}, {'#', 0x14, 0x7f, 0x14, 0x7f, 0x14}};
        static const uint8_t blank[5] = {};
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= '0' && c <= '9') return font[c - '0'];
        if (c >= 'A' && c <= 'Z') return font[10 + c - 'A'];
        for (auto& p : punct) if (p[0] == c) return p + 1;
        return blank;
    }
    static unsigned n14(const uint8_t* p) { return (unsigned)(p[0] & 127) << 7 | (p[1] & 127); }
    static uint16_t color(const uint8_t* p) { return (uint16_t)((p[0] & 3) << 14 | (p[1] & 127) << 7 | (p[2] & 127)); }
    uint16_t pal[16] = {}; uint32_t blitBytes = 0;
    void fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { for (unsigned j = y; j < y + h && j < 240; j++) for (unsigned i = x; i < x + w && i < 240; i++) px[j * 240 + i] = c; }
    // returns true if the frame is a screen command
    bool sysex(const uint8_t* m, size_t n) {
        if (n < 8 || m[0] != 0xf0 || m[1] != 0x7d || m[2] != 'D' || m[3] != 'A' || m[4] != 'W' || n > 300) return false;
        frames++; bytes += (uint32_t)n;
        const uint8_t* p = m + 7; size_t len = n - 8;
        return cmd(m[6], p, len);
    }
    bool cmd(unsigned c, const uint8_t* p, size_t len) {
        switch (c) {
        case 10: { size_t i = 0; while (i + 3 <= len) { unsigned l = n14(p + i); if (l < 1 || i + 2 + l > len) break; cmd(p[i + 2], p + i + 3, l - 1); i += 2 + l; } } break;
        case 2: if (len >= 3) fill(0, 0, 240, 240, color(p)); break;
        case 3: if (len >= 11) { unsigned x = n14(p), y = n14(p + 2), w = n14(p + 4), h = n14(p + 6); if (w && h && x + w <= 240 && y + h <= 240) fill(x, y, w, h, color(p + 8)); } break;
        case 4: if (len >= 11) {
            unsigned x = n14(p), y = n14(p + 2), sc = p[4]; uint16_t fg = color(p + 5), bg = color(p + 8);
            if (sc < 1 || sc > 4) break;
            for (size_t i = 11; i < len; i++, x += 6 * sc) {
                if (x + 6 * sc > 240 || y + 8 * sc > 240) continue;
                const uint8_t* g = glyph(p[i]);
                for (unsigned py = 0; py < 8 * sc; py++) for (unsigned qx = 0; qx < 6 * sc; qx++) {
                    unsigned row = py / sc, cl = qx / sc;
                    px[(y + py) * 240 + x + qx] = (cl < 5 && row < 7 && (g[cl] >> row & 1)) ? fg : bg;
                }
            } } break;
        case 8: for (unsigned i = 0; i < 16 && 3 * i + 3 <= len; i++) pal[i] = color(p + 3 * i); break;
        case 9: if (len >= 8) {   // same decoder as CMD_BLIT in app/ctrl.c
            unsigned x = n14(p), y = n14(p + 2), w = n14(p + 4), h = n14(p + 6), n = 0, total = w * h;
            if (!w || !h || total > 768 || x + w > 240 || y + h > 240) break;
            std::vector<uint16_t> cell(total);
            for (size_t i = 8; i < len && n < total; i++) {
                unsigned col = p[i] >> 3 & 15, run = (p[i] & 7) + 1;
                if ((p[i] & 7) == 7 && i + 1 < len) run = 8 + p[++i];
                while (run-- && n < total) cell[n++] = pal[col];
            }
            if (n == total) { for (unsigned j = 0; j < h; j++) for (unsigned k = 0; k < w; k++) px[(y + j) * 240 + x + k] = cell[j * w + k]; blitBytes += (uint32_t)n; }
        } break;
        default: break;
        }
        return true;
    }
    void ppm(const char* path, int zoom = 2) const {
        FILE* f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", 240 * zoom, 240 * zoom);
        for (int y = 0; y < 240 * zoom; y++) for (int x = 0; x < 240 * zoom; x++) {
            uint16_t c = px[(y / zoom) * 240 + x / zoom]; uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
};
