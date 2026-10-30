// Drives SamplerApp + SamplerUI with touches and dumps the draw ops of several screens as JSON (render_ui.py draws them).
#include <Session.h>
#include <SamplerApp.h>
#include <SamplerUI.h>
#include <Common.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <cstdio>
#include <string>
static std::string esc(const std::string& s) { std::string o; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o; }
int main(int argc, char** argv) {
    Denormals::enableOnThisThread();
    const int sr = SAMPLERATE;
    Session s(sr); s.setDataDir("/tmp/fm1sampler_test"); s.setSampleDir(std::string(getenv("HOME")) + "/fm1-sampler/samples");
    SamplerApp app(s, [](const uint8_t*, size_t) {});
    SamplerUI ui(app);
    const float W = 860, H = 400;
    ui.setSize(W, H, 0, 0); ui.setLink(true, true);
    std::vector<float> buf(512), in(512, 0.f);
    uint32_t now = 0;
    auto run = [&](double sec) { for (long i = 0; i < (long)(sec * sr) / 256; i++) { std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); if (i % 5 == 0) { now += 33; app.tick(now); } } };
    auto tapAt = [&](float x, float y) { ui.touch(0, x, y, now); ui.touch(2, x, y, now); run(0.05); };
    run(2.5);
    for (int i = 0; i < 4; i++) { app.playNote(60 + i * 4, true); run(0.3); app.playNote(60 + i * 4, false); run(0.1); }
    // a loop on two tracks: the first take on track 1, an overdub of notes on track 2
    app.loopRecord(); run(0.1); { int sq[] = {60, 64, 67, 72, 67, 64}; for (int n : sq) { app.playNote(n, true); run(0.45); app.playNote(n, false); run(0.15); } } app.loopRecord(); run(0.5);
    app.selectTrack(1); app.setAlgo(1); app.loopRecord(); run(0.2); { int sq[] = {48, 55}; for (int n : sq) { app.playNote(n, true); run(0.9); app.playNote(n, false); run(0.6); } } app.loopRecord(); run(1.0); app.selectTrack(0);
    FILE* f = fopen(argc > 1 ? argv[1] : "ui_ops.json", "w");
    fprintf(f, "{\"W\":%g,\"H\":%g,\"screens\":[", W, H);
    int count = 0;
    auto dump = [&](const char* name) {
        run(0.2); ui.frame(now);
        fprintf(f, "%s{\"name\":\"%s\",\"ops\":[", count++ ? "," : "", name);
        bool first = true;
        for (auto& o : ui.ops()) { fprintf(f, "%s[%d,%g,%g,%g,%g,%g,%g,%u,\"%s\"]", first ? "" : ",", (int)o.type, o.f[0], o.f[1], o.f[2], o.f[3], o.f[4], o.f[5], o.color, esc(o.text).c_str()); first = false; }
        fprintf(f, "]}");
    };
    const float rx = W - 252, tw = (252 - 8 - 16) / 5.f;
    auto tab = [&](int i) { tapAt(rx + (i % 5) * (tw + 4) + tw / 2, 44 + 8 + (i / 5) * 32 + 14); };
    ui.frame(now); dump("sample");
    tab(1); ui.frame(now); dump("fx");
    tab(2); ui.frame(now); dump("env");
    tab(3); ui.frame(now); dump("lfo");
    tab(6); ui.frame(now); dump("mix");
    tab(7); ui.frame(now); dump("key");
    tab(8); ui.frame(now); dump("loop");
    tab(9); ui.frame(now); dump("len");
    tab(0);
    app.playNote(60, true); app.playNote(67, true); run(0.5); ui.frame(now); dump("voices"); app.playNote(60, false); app.playNote(67, false); run(0.3);
    tapAt(60, 100); run(1.0); ui.frame(now); dump("track2");
    app.micSample(); in.assign(512, 0.01f); run(0.4); ui.frame(now); dump("mic_wait");
    { double ph = 0; for (long i = 0; i < 3 * sr / 256; i++) { for (int k = 0; k < 256; k++) { double env = 0.15 + 0.6 * std::fabs(std::sin(i * 256.0 / sr * 2.2)); float v = (float)(env * std::sin(ph)); ph += 0.05; in[2 * k] = in[2 * k + 1] = v; } std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); if (i % 5 == 0) { now += 33; app.tick(now); } } }
    ui.frame(now); dump("mic_rec");
    app.micSample(); in.assign(512, 0.f); run(2.0); app.tick(now + 100);
    tapAt(176 + 8 + 100, 44 + 8 + 66 + 142 + 6 + 10 + 12 + 17); ui.frame(now); dump("library");
    fprintf(f, "]}\n"); fclose(f);
    return 0;
}
