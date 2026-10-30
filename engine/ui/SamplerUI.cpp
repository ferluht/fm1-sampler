#include "SamplerUI.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace theme;

namespace {
float clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }
std::string num(const char* f, double v) { char b[48]; snprintf(b, sizeof b, f, v); return b; }
const SamplerApp::Page kTabs[] = {SamplerApp::P_HOME, SamplerApp::P_FX, SamplerApp::P_ENV, SamplerApp::P_LFO, SamplerApp::P_ARP, SamplerApp::P_SEQ, SamplerApp::P_MIX, SamplerApp::P_KEY, SamplerApp::P_SAVE, SamplerApp::P_LEN};
const char* kTabName[] = {"SAMPLE", "FX", "ENV", "LFO", "ARP", "TAPE", "MIX", "KEY", "LOOP", "LEN"};
}  // namespace

SamplerUI::SamplerUI(SamplerApp& app) : app_(app) {}

// ---- primitives ----
void SamplerUI::rrect(Rect r, float rad, uint32_t c) { UiOp o; o.type = UiOp::RRECT; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; o.f[4] = rad; o.color = c; ops_.push_back(o); }
void SamplerUI::stroke(Rect r, float rad, uint32_t c, float w) { UiOp o; o.type = UiOp::STROKE; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; o.f[4] = rad; o.f[5] = w; o.color = c; ops_.push_back(o); }
void SamplerUI::text(float x, float cy, float size, uint32_t c, const std::string& s, int align, bool bold) {
    UiOp o; o.type = UiOp::TEXT; o.f[0] = x; o.f[1] = cy + size * 0.36f; o.f[2] = size; o.f[3] = (float)align; o.f[4] = bold; o.color = c; o.text = s; ops_.push_back(o);
}
void SamplerUI::line(float x1, float y1, float x2, float y2, uint32_t c, float w) { UiOp o; o.type = UiOp::LINE; o.f[0] = x1; o.f[1] = y1; o.f[2] = x2; o.f[3] = y2; o.f[4] = w; o.color = c; ops_.push_back(o); }
void SamplerUI::circle(float cx, float cy, float r, uint32_t c) { UiOp o; o.type = UiOp::CIRCLE; o.f[0] = cx; o.f[1] = cy; o.f[2] = r; o.color = c; ops_.push_back(o); }
void SamplerUI::tri(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c) { UiOp o; o.type = UiOp::TRI; o.f[0] = x1; o.f[1] = y1; o.f[2] = x2; o.f[3] = y2; o.f[4] = x3; o.f[5] = y3; o.color = c; ops_.push_back(o); }
void SamplerUI::clip(Rect r) { UiOp o; o.type = UiOp::CLIP; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; ops_.push_back(o); }
void SamplerUI::endClip() { UiOp o; o.type = UiOp::ENDCLIP; ops_.push_back(o); }
void SamplerUI::hit(Rect r, std::function<void(float, float)> d, std::function<void(float, float)> m, std::function<void()> u) { hits_.push_back({r, std::move(d), std::move(m), std::move(u)}); }
void SamplerUI::tap(Rect r, std::function<void()> f) { hit(r, [f](float, float) { f(); }); }
std::string SamplerUI::fit(const std::string& s, float width, float size) const {
    const size_t maxc = (size_t)std::max(1.f, width / (size * 0.60f));
    return s.size() <= maxc ? s : s.substr(0, maxc > 1 ? maxc - 1 : 1) + "\xE2\x80\xA6";
}
void SamplerUI::touch(int action, float x, float y, uint32_t now) {
    now_ = now;
    if (action == 0) {
        haveActive_ = false;
        for (size_t i = hits_.size(); i-- > 0;) if (hits_[i].r.has(x, y)) { active_ = hits_[i]; haveActive_ = true; if (active_.down) active_.down(x, y); break; }
    } else if (action == 1) {
        if (haveActive_ && active_.move) active_.move(x, y);
    } else if (haveActive_) {
        if (active_.up) active_.up();
        haveActive_ = false;
    }
}
void SamplerUI::button(Rect r, const std::string& label, bool on, std::function<void()> f, float size, bool accent) {
    rrect(r, R_CONTROL, on ? ACCENT : SURFACE);
    if (!on) stroke(r, R_CONTROL, accent ? ACCENT : OUTLINE, 1);
    text(r.x + r.w / 2, r.y + r.h / 2, size, on ? ON_ACCENT : accent ? ACCENT : TEXT, fit(label, r.w - 6, size), 1, true);
    tap(r, std::move(f));
}

// ---- frame ----
void SamplerUI::frame(uint32_t now) {
    now_ = now;
    st_ = app_.status();
    ops_.clear(); hits_.clear();
    rrect({0, 0, W_, H_}, 0, BG);
    const float tb = 44, left = 176, right = 252;
    topBar({0, 0, W_, tb});
    trackList({MARGIN, tb + MARGIN, left - MARGIN, H_ - tb - 2 * MARGIN});
    sampleArea({left + MARGIN, tb + MARGIN, W_ - left - right - 2 * MARGIN, H_ - tb - 2 * MARGIN});
    pagePanel({W_ - right, tb + MARGIN, right - MARGIN, H_ - tb - 2 * MARGIN});
    if (sheetOpen_) librarySheet();
}

void SamplerUI::topBar(Rect r) {
    rrect(r, 0, SURFACE);
    line(0, r.h, W_, r.h, OUTLINE, 1);
    const float cy = r.h / 2;
    float x = MARGIN;
    // PLAY / STOP
    Rect pl{x, 6, 44, r.h - 12};
    rrect(pl, R_CONTROL, RAISED);
    if (st_.playing) { rrect({pl.x + 15, cy - 8, 5, 16}, 0, ACCENT); rrect({pl.x + 24, cy - 8, 5, 16}, 0, ACCENT); }
    else tri(pl.x + 16, cy - 9, pl.x + 16, cy + 9, pl.x + 30, cy, TEXT);
    tap(pl, [this] { app_.transportPlay(); });
    x += 52;
    // REC: arms the first take on a clean looper, overdub afterwards
    Rect rc{x, 6, 74, r.h - 12};
    const bool recOn = st_.freeRecording || st_.track_clip_rec[app_.track()], armed = st_.freeArmed;
    rrect(rc, R_CONTROL, recOn ? ACCENT : RAISED);
    if (armed) stroke(rc, R_CONTROL, ACCENT, 2);
    circle(rc.x + 18, cy, 6, recOn ? ON_ACCENT : ACCENT);
    text(rc.x + 28, cy, 9.5f, recOn ? ON_ACCENT : TEXT, app_.loopExists() ? "OVERDUB" : "REC", 0, true);
    tap(rc, [this] { app_.loopRecord(); });
    x += 82;
    button({x, 6, 56, r.h - 12}, "SAVE", false, [this] { app_.loopSave(); }, 11);
    x += 62;
    button({x, 6, 56, r.h - 12}, "CLEAR", false, [this] { app_.loopClear(); }, 11);
    x += 70;
    text(x, cy - 6, 12, st_.freeRecording || recOn || armed ? ACCENT : TEXT, app_.loopText(), 0, true);
    text(x, cy + 11, 9.5f, DIM, std::string("LOOP SOURCE: ") + (app_.knobs(SamplerApp::P_SAVE)[1].value), 0, false);
    tap({x - 4, 0, 230, r.h}, [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
    // link state and load
    const uint32_t lc = linkMidi_ && linkAudio_ ? TEXT : linkMidi_ ? ACCENT : DIM;
    circle(W_ - 22, cy, 5, lc);
    text(W_ - 34, cy - 6, 10, DIM, linkMidi_ ? "FM-1" : "NO FM-1", 2, true);
    text(W_ - 34, cy + 9, 9, DIM, "CPU " + num("%.0f", st_.cpu_permille / 10.0) + "%", 2, false);
}

void SamplerUI::trackList(Rect r) {
    const float rh = (r.h - 7 * 4) / 8;
    for (int i = 0; i < SamplerApp::kTracks; i++) {
        Rect row{r.x, r.y + i * (rh + 4), r.w, rh};
        const bool sel = i == app_.track();
        rrect(row, R_CARD, sel ? RAISED : SURFACE);
        stroke(row, R_CARD, sel ? ACCENT : OUTLINE, sel ? 2 : 1);
        text(row.x + 14, row.y + rh / 2, T_TITLE, sel ? ACCENT : TEXT, std::to_string(i + 1), 1, true);
        const std::string lab = app_.sampleLabel(i);
        text(row.x + 30, row.y + rh / 2 - 6, 11, lab.empty() ? DIM : TEXT, fit(lab.empty() ? "EMPTY" : lab, r.w - 40, 11), 0, true);
        // level and loop mark
        const float lv = std::min(1.f, st_.track_peak[i] * 1.5f);
        rrect({row.x + 30, row.y + rh - 12, r.w - 44, 3}, 0, OUTLINE);
        rrect({row.x + 30, row.y + rh - 12, (r.w - 44) * lv, 3}, 0, TEXT);
        if (st_.track_clip_rec[i]) circle(row.x + r.w - 10, row.y + 10, 4, ACCENT);
        else if (st_.track_has_clip[i]) circle(row.x + r.w - 10, row.y + 10, 3.5f, TEXT);
        tap(row, [this, i] { app_.selectTrack(i); });
    }
}

void SamplerUI::sampleArea(Rect r) {
    const int t = app_.track();
    const auto ts = app_.state(t);
    const std::string lab = app_.sampleLabel(t);
    text(r.x, r.y + 12, T_TITLE, lab.empty() ? DIM : TEXT, fit(lab.empty() ? "EMPTY TRACK: PICK A SAMPLE" : lab, r.w - 120, T_TITLE), 0, true);
    text(r.x + r.w, r.y + 12, T_CAPTION, DIM, "TRACK " + std::to_string(t + 1), 2, true);
    // algorithm
    const float cw = (r.w - 5 * 4) / 6;
    for (int a = 0; a < 6; a++) {
        Rect c{r.x + a * (cw + 4), r.y + 30, cw, 28};
        button(c, a == 5 ? "PAUL" : SamplerApp::algoName(a), ts.algo == a, [this, a] { app_.setAlgo(a); }, 9.5f);
    }
    // waveform with the playback window
    Rect w{r.x, r.y + 66, r.w, 118};
    rrect(w, R_CARD, SURFACE); stroke(w, R_CARD, OUTLINE, 1);
    const int bins = std::max(20, (int)(w.w / 3));
    if (app_.micState() == 1 || app_.micState() == 2) {   // microphone: input level + trigger threshold while waiting, the growing take while recording
        const float cy0 = w.y + w.h / 2, half = w.h / 2 - 8;
        if (app_.micState() == 1) {
            const float thr = std::min(1.f, app_.micThreshold() * 2.f), lv = std::min(1.f, st_.inputPeak * 2.f);
            rrect({w.x + 8, cy0 - lv * half, w.w - 16, std::max(2.f, lv * half * 2)}, 0, lv > thr ? ACCENT : OUTLINE);
            line(w.x + 8, cy0 - thr * half, w.x + w.w - 8, cy0 - thr * half, ACCENT, 1);
            line(w.x + 8, cy0 + thr * half, w.x + w.w - 8, cy0 + thr * half, ACCENT, 1);
            text(w.x + w.w / 2, w.y + w.h - 12, T_CAPTION, DIM, "WAITING FOR SOUND ABOVE THE LINES  DRAG TO SET  " + num("%.0f DB", 20 * std::log10(app_.micThreshold())), 1, false);
            auto setThr = [this, half, cy0](float, float y) { app_.setMicThreshold01(std::log10(std::max(0.0025f, std::fabs(y - cy0) / half / 2.f) / 0.005f) / 2.f); };
            hit(w, setThr, setThr);
        } else {
            float live[2048]; const int n = app_.session().livePeaks(live, 2048);
            const int cols = std::max(20, (int)(w.w / 3)), per = std::max(1, (n + cols - 1) / cols);   // longer takes are squeezed to fit
            const float bw0 = (w.w - 16) / std::max(cols, (n + per - 1) / per);
            for (int i = 0; i * per < n; i++) {
                float v = 0; for (int j = i * per; j < std::min(n, (i + 1) * per); j++) v = std::max(v, live[j]);
                v = std::min(1.f, v * 1.1f);
                rrect({w.x + 8 + i * bw0, cy0 - std::max(1.f, v * half), std::max(1.f, bw0 - 1), std::max(2.f, v * half * 2)}, 0, TEXT);
            }
            text(w.x + w.w - 24, w.y + 16, T_CAPTION, ACCENT, "REC " + std::to_string(st_.sampleSeconds).substr(0, 4) + "S", 2, true);
        }
        goto after_wave;
    }
    {
    auto pk = app_.peaks(t, bins);
    const float cy = w.y + w.h / 2, bw = w.w / bins;
    for (int i = 0; i < (int)pk.size(); i++) {
        const float pos = (i + 0.5f) / bins, v = std::min(1.f, pk[(size_t)i] * 1.1f);
        const bool in = pos >= ts.start && pos <= ts.end;
        rrect({w.x + i * bw + 0.5f, cy - std::max(1.f, v * (w.h / 2 - 8)), std::max(1.f, bw - 1), std::max(2.f, v * (w.h - 16))}, 0, in ? TEXT : OUTLINE);
    }
    if (pk.empty()) text(w.x + w.w / 2, cy, T_BODY, DIM, "NO SAMPLE", 1, false);
    for (int j = 0; j < st_.voiceN[t]; j++) {   // where the sounding voices are reading
        const float vx = w.x + 2 + st_.voicePos[t][j] * (w.w - 4);
        line(vx, w.y + 22, vx, w.y + w.h - 10, ACCENT, 1.5f);
        circle(vx, w.y + w.h - 10, 3.5f, ACCENT);
    }
    }
    after_wave:
    // start / end handles (not while the microphone take is shown)
    for (int h = 0; h < (app_.micState() == 1 || app_.micState() == 2 ? 0 : 2); h++) {
        const float v = h ? ts.end : ts.start, hx = w.x + 2 + v * (w.w - 4);
        line(hx, w.y + 4, hx, w.y + w.h - 4, ACCENT, 2);
        rrect({hx - (h ? 10 : 0), w.y + 4, 10, 14}, 1, ACCENT);
        hit({hx - 18, w.y, 36, w.h},
            [](float, float) {},
            [this, h, w](float x, float) { auto k = app_.knobs(SamplerApp::P_HOME); k[(size_t)h].set(clamp01((x - w.x - 2) / (w.w - 4))); });
    }
    // loop strip under the waveform
    Rect ls{r.x, w.y + w.h + 6, r.w, 34};
    rrect(ls, 0, SURFACE); stroke(ls, 0, OUTLINE, 1);
    {   // the whole loop: every track mixed (dim) and the selected track's own loop on top
        float mixp[400], trp[400]; const int ln = app_.session().loopPeaks(-1, mixp, 400); app_.session().loopPeaks(t, trp, 400);
        if (ln > 0) {
            float mx = 0.08f; for (int i = 0; i < ln; i++) mx = std::max(mx, mixp[i]);
            const float lcy = ls.y + ls.h / 2, lh = ls.h / 2 - 3, lbw = (ls.w - 4) / ln;
            for (int i = 0; i < ln; i++) {
                const float v = std::min(1.f, mixp[i] / mx), u = std::min(1.f, trp[i] / mx);
                if (v > 0.01f) rrect({ls.x + 2 + i * lbw, lcy - v * lh, std::max(1.f, lbw), std::max(1.f, 2 * v * lh)}, 0, OUTLINE);
                if (u > 0.01f) rrect({ls.x + 2 + i * lbw, lcy - u * lh, std::max(1.f, lbw), std::max(1.f, 2 * u * lh)}, 0, TEXT);
            }
        } else text(ls.x + ls.w / 2, ls.y + ls.h / 2, T_CAPTION, DIM, "NO LOOP: PRESS REC", 1, false);
    }
    const float lp = st_.track_has_clip[t] ? st_.clipPos[t] : (app_.loopExists() && st_.loopSamples > 0 ? (float)(st_.pos / st_.loopSamples) : 0.f);
    if (app_.loopExists() || st_.freeRecording) rrect({ls.x + lp * (ls.w - 4), ls.y - 2, 3, ls.h + 4}, 0, st_.freeRecording || st_.track_clip_rec[t] ? ACCENT : TEXT);
    // library row
    Rect lr{r.x, ls.y + ls.h + 12, r.w, 34};
    const auto& lib = app_.library();
    const int cur = app_.libraryCursor();
    button({lr.x, lr.y, 34, lr.h}, "<", false, [this] { app_.browse(-1); }, 15);
    button({lr.x + lr.w - 34 - 0, lr.y, 34, lr.h}, ">", false, [this] { app_.browse(1); }, 15);
    Rect nm{lr.x + 40, lr.y, lr.w - 80, lr.h};
    rrect(nm, R_CARD, SURFACE); stroke(nm, R_CARD, OUTLINE, 1);
    if (!lib.empty()) {
        text(nm.x + 10, nm.y + nm.h / 2 - 6, 12, TEXT, fit(lib[(size_t)cur].label, nm.w - 80, 12), 0, true);
        text(nm.x + 10, nm.y + nm.h / 2 + 9, 9, DIM, lib[(size_t)cur].group + "  " + std::to_string(cur + 1) + "/" + std::to_string(lib.size()), 0, false);
    }
    text(nm.x + nm.w - 10, nm.y + nm.h / 2, 9.5f, ACCENT, "LIBRARY", 2, true);
    tap(nm, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); });
    // microphone and disk
    Rect br{r.x, lr.y + lr.h + 8, r.w, 38};
    const int ms = app_.micState();
    static const char* micLab[] = {"RECORD FROM MIC", "WAITING FOR SOUND...", "STOP AND KEEP", "SAVING..."};
    button({br.x, br.y, br.w * 0.6f - 3, br.h}, micLab[ms], ms == 2, [this] { app_.micSample(); }, 12, ms != 0);
    button({br.x + br.w * 0.6f + 3, br.y, br.w * 0.4f - 3, br.h}, "LOAD FILE FROM DISK", false, [this] { app_.requestFile(); }, 11);
    const std::string ts_ = app_.toast();
    if (!ts_.empty()) text(r.x, br.y + br.h + 14, 11, ACCENT, ts_, 0, true);
}

void SamplerUI::pagePanel(Rect r) {
    const float tw = (r.w - 4 * 4) / 5;
    for (int i = 0; i < 10; i++) {
        Rect c{r.x + (i % 5) * (tw + 4), r.y + (i / 5) * 32, tw, 28};
        button(c, kTabName[i], app_.page() == kTabs[i], [this, i] { app_.gotoPage(kTabs[i]); }, 9.5f);
    }
    auto k = app_.knobs();
    const float top = r.y + 72, rh = (r.h - 72) / 4;
    for (int i = 0; i < 4; i++) {
        const auto& p = k[(size_t)i];
        const float y = top + i * rh;
        if (!p.valid) continue;
        text(r.x, y + 8, T_CAPTION, DIM, "K" + std::to_string(i + 1) + "  " + p.name, 0, true);
        text(r.x + r.w, y + 10, T_VALUE, p.isChoice ? ACCENT : TEXT, p.value, 2, true);
        Rect s{r.x, y + 26, r.w, 14};
        rrect(s, 0, SURFACE); stroke(s, 0, OUTLINE, 1);
        if (p.isChoice) {   // segments are not known here: a notch at the current position
            rrect({s.x + p.norm * (s.w - 8), s.y, 8, s.h}, 0, ACCENT);
        } else {
            rrect({s.x, s.y, std::max(2.f, s.w * clamp01(p.norm)), s.h}, 0, TEXT);
        }
        Rect hr{s.x, y + 14, s.w, 40};
        hit(hr, [this, i, hr](float x, float) { app_.knobs()[(size_t)i].set(clamp01((x - hr.x) / hr.w)); },
            [this, i, hr](float x, float) { app_.knobs()[(size_t)i].set(clamp01((x - hr.x) / hr.w)); });
    }
}

void SamplerUI::librarySheet() {
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(BG, 200));
    tap({0, 0, W_, H_}, [this] { sheetOpen_ = false; });
    const Rect box{W_ * 0.22f, 24, W_ * 0.56f, H_ - 48};
    rrect(box, R_CARD, SURFACE); stroke(box, R_CARD, ACCENT, 1);
    tap(box, [] {});
    text(box.x + 14, box.y + 20, T_TITLE, TEXT, "SAMPLE LIBRARY", 0, true);
    button({box.x + box.w - 70, box.y + 6, 60, 28}, "CLOSE", false, [this] { sheetOpen_ = false; }, 10);
    sheetList_ = {box.x + 6, box.y + 44, box.w - 12, box.h - 52};
    const auto& lib = app_.library();
    // rows: a header per group, then its entries
    struct Row { bool head; std::string text; int entry; };
    std::vector<Row> rows;
    std::string g;
    for (int i = 0; i < (int)lib.size(); i++) { if (lib[(size_t)i].group != g) { g = lib[(size_t)i].group; rows.push_back({true, g, -1}); } rows.push_back({false, lib[(size_t)i].label, i}); }
    const float rh = 34;
    sheetScroll_ = std::min(std::max(0.f, sheetScroll_), std::max(0.f, rows.size() * rh - sheetList_.h));
    clip(sheetList_);
    for (size_t i = 0; i < rows.size(); i++) {
        const float y = sheetList_.y + i * rh - sheetScroll_;
        if (y + rh < sheetList_.y || y > sheetList_.y + sheetList_.h) continue;
        if (rows[i].head) { text(sheetList_.x + 8, y + rh / 2, T_CAPTION, DIM, rows[i].text, 0, true); line(sheetList_.x, y + rh - 3, sheetList_.x + sheetList_.w, y + rh - 3, OUTLINE, 1); continue; }
        const bool cur = rows[i].entry == app_.libraryCursor();
        if (cur) { rrect({sheetList_.x, y + 1, sheetList_.w, rh - 2}, R_CONTROL, RAISED); stroke({sheetList_.x, y + 1, sheetList_.w, rh - 2}, R_CONTROL, ACCENT, 1); }
        text(sheetList_.x + 14, y + rh / 2, T_BODY + 1, cur ? ACCENT : TEXT, fit(rows[i].text, sheetList_.w - 30, T_BODY + 1), 0, cur);
    }
    endClip();
    hit(sheetList_, [this](float, float y) { sheetLastY_ = y; sheetMoved_ = 0; },
        [this](float, float y) { sheetScroll_ -= y - sheetLastY_; sheetMoved_ += std::fabs(y - sheetLastY_); sheetLastY_ = y; },
        [this, rows, rh]() {
            if (sheetMoved_ > 8) return;
            const int idx = (int)((sheetLastY_ - sheetList_.y + sheetScroll_) / rh);
            if (idx >= 0 && idx < (int)rows.size() && !rows[(size_t)idx].head) { app_.loadEntry(rows[(size_t)idx].entry); sheetOpen_ = false; }
        });
}
