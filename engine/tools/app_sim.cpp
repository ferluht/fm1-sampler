// Scripted host session: a virtual FM-1 drives SamplerApp + Session; screenshots of its screen are written as PPM after each step.
#include <Session.h>
#include <SamplerApp.h>
#include <Common.h>
#include "fm1_screen.h"
#include <cmath>
#include <thread>
#include <chrono>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    Denormals::enableOnThisThread();
    const int sr = SAMPLERATE;
    Session s(sr);
    s.setDataDir("/tmp/fm1sampler_test"); s.setSampleDir(std::string(getenv("HOME")) + "/fm1-sampler/samples");
    system("mkdir -p /tmp/fm1sampler_test");
    Fm1Screen scr;
    SamplerApp app(s, [&](const uint8_t* m, size_t n) { scr.sysex(m, n); });
    std::vector<float> buf(512), in(512, 0.f), all;
    uint32_t now = 0; double mf = 0, mic = 0;
    auto run = [&](int ms) {
        for (int t = 0; t < ms; t += 16) {
            for (int b = 0; b < 16 * sr / 1000 / 256 + (t & 16 ? 1 : 0); b++) {
                for (int k = 0; k < 256; k++) { float v = mic > 0 ? 0.4f * std::sin(mf) : 0.f; mf += 2 * M_PI * mic / sr; in[2 * k] = in[2 * k + 1] = v; }
                std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); all.insert(all.end(), buf.begin(), buf.end());
            }
            now += 16; app.tick(now);
        }
    };
    auto note = [&](int n, bool on) { uint8_t m[3] = {(uint8_t)(on ? 0x90 : 0x80), (uint8_t)n, 100}; app.onMidi(m, 3); };
    auto btn = [&](int b) { uint8_t m[3] = {0x91, (uint8_t)b, 100}; app.onMidi(m, 3); m[0] = 0x81; app.onMidi(m, 3); };
    auto enc = [&](int i, int d) { uint8_t m[3] = {0xb0, (uint8_t)(16 + i), (uint8_t)(64 + d)}; app.onMidi(m, 3); };
    int shot = 0;
    auto snap = [&](const char* name) { run(300); char p[128]; snprintf(p, sizeof p, "app_%02d_%s.ppm", shot++, name); scr.ppm(p); auto st = app.status(); printf("%s  playing=%d loop=%d clip0=%d cpu=%u\n", p, st.playing, (int)app.loopExists(), st.track_has_clip[0], st.cpu_permille); };
    auto rms = [&](size_t from) { double a = 0; size_t n = 0; for (size_t i = from; i < all.size(); i++) { a += all[i] * all[i]; n++; } return n ? std::sqrt(a / n) : 0.0; };

    uint8_t sc[] = {0xf0, 0x7d, 'D', 'A', 'W', 1, 0x41, 3, 0, 3, 0, 0xf7}; app.onSysex(sc, sizeof sc);
    run(2500); snap("home");
    size_t mark = all.size(); note(60, true); run(600); note(60, false); run(300);
    printf("keys: rms %.4f\n", rms(mark));
    // knobs of every page
    btn(SamplerApp::FX); enc(2, 5); enc(3, 5); enc(4, 8); enc(5, 5); snap("fx");
    btn(SamplerApp::ENV); enc(2, 4); enc(3, -3); enc(4, -3); enc(5, 6); snap("env");
    btn(SamplerApp::LFO); enc(3, 20); enc(4, 1); snap("lfo");
    btn(SamplerApp::HOME); enc(2, 20); enc(3, -15); enc(4, 5); enc(5, 12); run(800); snap("home_edit");
    btn(SamplerApp::ARP); enc(2, 8); enc(3, 1); snap("arp");
    mark = all.size(); note(60, true); note(64, true); note(67, true); run(1500); note(60, false); note(64, false); note(67, false); printf("arp: rms %.4f\n", rms(mark));
    enc(2, -20); run(200);
    btn(SamplerApp::SEQ); enc(2, 3); enc(3, 10); enc(4, 8); enc(5, 6); snap("seq");
    btn(SamplerApp::EDIT); enc(2, -4); enc(3, 6); snap("mix"); enc(2, -8); enc(3, -5); snap("mix_down");
    // select, presets, algorithm
    enc(0, 1); enc(1, 1); run(200); btn(SamplerApp::HOME); snap("track2_algo");
    enc(0, -1); enc(6, 1); run(600); snap("presets_audition");
    enc(6, 1); enc(6, 1); run(1500); snap("presets_audition2");
    // looper: REC arms, first note starts the take, REC ends it, overdub, save
    btn(SamplerApp::PLAY); run(200); btn(SamplerApp::PLAY);
    btn(SamplerApp::REC); run(200); snap("rec_armed");
    int seq[] = {60, 64, 67, 72};
    for (int i = 0; i < 4; i++) { note(seq[i], true); run(350); note(seq[i], false); run(150); }
    btn(SamplerApp::REC); run(300); snap("loop_playing");
    btn(SamplerApp::SEL); enc(2, 1); run(200); snap("len_x2"); enc(2, -1); run(400);
    btn(SamplerApp::REC); run(100); note(55, true); run(300); note(55, false); run(1200); btn(SamplerApp::REC); snap("overdub");
    { uint8_t m[3] = {0x91, (uint8_t)SamplerApp::SAVE, 100}; app.onMidi(m, 3); m[0] = 0x81; app.onMidi(m, 3); } run(500); snap("saved");
    enc(6, 1); // library has the saved loop now
    // key / GLO
    btn(SamplerApp::GLO); enc(2, 2); enc(3, 3); snap("key");
    // mic sampling: wait for sound, record, stop, loads into the selected track
    btn(SamplerApp::SAVE); run(100); enc(2, 1); run(300); mic = 0.02; run(300); snap("mic_wait"); enc(2, 4); snap("mic_thr"); enc(2, -4); mic = 330; run(1200); mic = 0; snap("mic_recording"); enc(2, -1); run(2500); snap("mic_done");
    auto st = app.status();
    printf("final: sampleVersion=%d library=%zu\n", st.sampleVersion, app.library().size());
    FILE* f = fopen("app_out.raw", "wb"); fwrite(all.data(), 4, all.size(), f); fclose(f);
    return 0;
}
