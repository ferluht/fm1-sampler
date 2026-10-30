// Host tests of the sampler app: every algorithm sounds, LFO targets, stretch/pitch, tape knobs, arp, loop speed, library, import. Exit code 1 on failure.
#include <Session.h>
#include <SamplerApp.h>
#include <Common.h>
#include <Stretch.h>
#include <Slicer.h>
#include <Reverb.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s: ", #c); printf(__VA_ARGS__); printf("\n"); } else { printf("ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    Denormals::enableOnThisThread();
    const int sr = SAMPLERATE;
    system("rm -rf /tmp/fm1sampler_t2; mkdir -p /tmp/fm1sampler_t2");
    Session s(sr); s.setDataDir("/tmp/fm1sampler_t2"); s.setSampleDir(std::string(getenv("HOME")) + "/fm1-sampler/samples");
    SamplerApp app(s, [](const uint8_t*, size_t) {});
    std::vector<float> buf(512), in(512, 0.f), all; uint32_t now = 0;
    auto run = [&](double sec) { for (long i = 0; i < (long)(sec * sr) / 256; i++) { std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); all.insert(all.end(), buf.begin(), buf.end()); if (i % 5 == 0) { now += 33; app.tick(now); } } };
    auto rmsFrom = [&](size_t from) { double a = 0; size_t n = 0; for (size_t i = from; i < all.size(); i++) { a += all[i] * all[i]; n++; } return n ? std::sqrt(a / n) : 0.0; };
    auto enc = [&](int i, int d) { uint8_t m[3] = {0xb0, (uint8_t)(16 + i), (uint8_t)(64 + d)}; app.onMidi(m, 3); };
    auto btn = [&](int b) { uint8_t m[3] = {0x91, (uint8_t)b, 100}; app.onMidi(m, 3); m[0] = 0x81; app.onMidi(m, 3); };
    auto play = [&](int note, double sec) { size_t m = all.size(); app.playNote(note, true); run(sec); app.playNote(note, false); run(0.3); return rmsFrom(m); };
    run(2.5);
    CHECK(app.hasSample(0) && app.hasSample(1) && app.hasSample(2) && app.hasSample(3), "four default tracks load (%s, %s, %s, %s)", app.sampleLabel(0).c_str(), app.sampleLabel(1).c_str(), app.sampleLabel(2).c_str(), app.sampleLabel(3).c_str());
    CHECK(!app.hasSample(4), "track 5 is empty");
    // algorithms on track 1
    for (int a = 0; a < 6; a++) { app.setAlgo(a); run(0.3); double r = play(60, 1.0); CHECK(r > 0.003, "algorithm %s sounds (rms %.4f)", SamplerApp::algoName(a), r); }
    app.setAlgo(0);
    // LFO on each target
    btn(SamplerApp::LFO);
    for (int tg = 0; tg < 4; tg++) { auto k = app.knobs(); while (app.state(0).lfoTarget < tg) k[2].step(1); enc(3, 40); run(0.2); double r = play(60, 1.2); CHECK(r > 0.002, "LFO target %d sounds (rms %.4f), lfo value %.2f", tg, r, app.status().lfoValue[0]); }
    enc(3, -80); run(0.3);
    // stretch + pitch (offline processing, applied when the knob rests)
    btn(SamplerApp::HOME); enc(4, 20); enc(5, 12); run(2.0); { double r = play(60, 1.0); CHECK(r > 0.002, "stretch + pitch sound (rms %.4f)", r); }
    enc(4, -20); enc(5, -12); run(2.0);
    // tape page
    btn(SamplerApp::SEQ); enc(3, 30); enc(4, 30); enc(5, 20); run(0.3); { double r = play(60, 1.0); CHECK(r > 0.002 && std::isfinite(r), "exciter + lofi + wobble sound (rms %.4f)", r); }
    // arp
    btn(SamplerApp::ARP); enc(2, 1); for (int i = 0; i < 6; i++) enc(2, 1); run(0.2);
    { size_t m = all.size(); app.playNote(60, true); app.playNote(64, true); app.playNote(67, true); run(1.5); app.playNote(60, false); app.playNote(64, false); app.playNote(67, false); run(0.3); CHECK(rmsFrom(m) > 0.002, "arp sounds (rms %.4f)", rmsFrom(m)); }
    for (int i = 0; i < 12; i++) enc(2, -1); run(0.2);
    // loop: free take, speed, overdub, save
    btn(SamplerApp::REC); run(0.1);
    int seq[] = {60, 64, 67, 72}; for (int n : seq) { app.playNote(n, true); run(0.3); app.playNote(n, false); run(0.1); }
    btn(SamplerApp::REC); run(0.5);
    CHECK(app.status().track_has_clip[0] && app.loopExists(), "first take became the loop (%.0f bpm, %d bars)", app.status().bpm, app.status().loopBars);
    size_t m = all.size(); run(2.0); CHECK(rmsFrom(m) > 0.003, "loop plays back (rms %.4f)", rmsFrom(m));
    btn(SamplerApp::SEQ); enc(2, -10); run(0.1); m = all.size(); run(2.0); CHECK(rmsFrom(m) > 0.001 && app.status().clipPos[0] >= 0.f, "loop at half speed plays (pos %.2f)", app.status().clipPos[0]);
    enc(2, 10); run(0.2);
    btn(SamplerApp::REC); run(0.1); app.playNote(55, true); run(0.3); app.playNote(55, false); run(2.0); btn(SamplerApp::REC); run(0.2);
    CHECK(!app.status().track_clip_rec[0], "overdub ended");
    { app.selectTrack(0); app.setAlgo(4); run(0.3); app.playNote(60, true); run(0.8); CHECK(app.status().voiceN[0] > 3, "granular: every grain is reported (%d)", app.status().voiceN[0]); app.playNote(60, false); run(0.5); app.setAlgo(0); run(0.2); }
    { const double L = app.status().loopSamples; btn(SamplerApp::SEL); enc(2, 1); run(0.3);
      CHECK(app.status().loopSamples == 2 * L, "LENGTH page: loop doubled (%.0f -> %.0f)", L, app.status().loopSamples);
      m = all.size(); run(2.5); CHECK(rmsFrom(m) > 0.003, "doubled loop plays (rms %.4f)", rmsFrom(m));
      enc(2, -1); run(0.3); CHECK(app.status().loopSamples == L, "LENGTH page: loop halved back (%.0f)", app.status().loopSamples); }
    app.playNote(60, true); run(0.3); CHECK(app.status().voiceN[0] > 0 && app.status().voicePos[0][0] > 0.f, "active sampler voices reported (%d, pos %.3f)", app.status().voiceN[0], app.status().voicePos[0][0]); app.playNote(60, false); run(0.5);
    { app.selectTrack(3); app.setAlgo(3); run(0.3); size_t km = all.size(); uint8_t kd[3] = {0x90, (uint8_t)(53 + 26), 100}, ku[3] = {0x80, (uint8_t)(53 + 26), 0}; app.onMidi(kd, 3); run(0.6); app.onMidi(ku, 3); run(0.3);
      CHECK(rmsFrom(km) > 0.002, "KIT: the last of the 27 keys plays a pad (rms %.4f)", rmsFrom(km)); app.selectTrack(0); }
    uint8_t sd[3] = {0x91, (uint8_t)SamplerApp::SAVE, 100}, su[3] = {0x81, (uint8_t)SamplerApp::SAVE, 100};
    app.onMidi(sd, 3); app.onMidi(su, 3); run(1.0);
    int wavs = 0; { FILE* p = popen("ls /tmp/fm1sampler_t2/sample_*.wav 2>/dev/null | wc -l", "r"); if (p) { fscanf(p, "%d", &wavs); pclose(p); } }
    CHECK(wavs == 1, "SAVE wrote the loop into the library (%d files)", wavs);
    app.onMidi(sd, 3); run(0.9); app.onMidi(su, 3); run(0.3);
    CHECK(!app.status().track_has_clip[0] && !app.loopExists(), "holding SAVE cleared the loop");
    { bool any = false; for (int t = 0; t < 8; t++) any |= app.status().track_has_clip[t]; CHECK(!any, "...and every other track's loop"); }
    // microphone sample
    btn(SamplerApp::SAVE); btn(SamplerApp::SAVE);   // (taps save again but there is no loop: just a message)
    enc(2, 1); run(0.3); CHECK(app.micState() == 1, "mic sampling waits for sound");
    { double ph = 0; for (long i = 0; i < (long)(1.0 * sr) / 256; i++) { for (int k = 0; k < 256; k++) { float v = 0.4f * std::sin(ph); ph += 2 * M_PI * 440.0 / sr; in[2 * k] = in[2 * k + 1] = v; } std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); if (i % 5 == 0) { now += 33; app.tick(now); } } std::fill(in.begin(), in.end(), 0.f); }
    CHECK(app.micState() == 2, "mic sampling recording");
    enc(2, -1); run(3.0);
    CHECK(app.micState() == 0 && app.library().back().group == "User", "mic sample saved into the library and loaded (%s)", app.sampleLabel(0).c_str());
    // import from disk: a WAV dropped into the data dir
    { std::ifstream src(std::string(getenv("HOME")) + "/fm1-sampler/samples/Bass/Fuzz.wav", std::ios::binary); std::ofstream dst("/tmp/fm1sampler_t2/sample_FromDisk.wav", std::ios::binary); dst << src.rdbuf(); }
    app.selectTrack(4); app.importFile("/tmp/fm1sampler_t2/sample_FromDisk.wav"); run(2.0);
    CHECK(app.hasSample(4) && app.sampleLabel(4) == "sample_FromDisk", "import from disk loads into the selected track (%s)", app.sampleLabel(4).c_str());
    // presets audition
    app.selectTrack(4); size_t pm = all.size(); enc(6, 1); run(1.5); CHECK(rmsFrom(pm) > 0.002, "browsing plays the sample (rms %.4f)", rmsFrom(pm));
    // DSP units: offline stretch / pitch and the slicer
    {
        const int N = 44100; std::vector<float> sine((size_t)N * 2);
        for (int i = 0; i < N; i++) sine[(size_t)i * 2] = sine[(size_t)i * 2 + 1] = 0.5f * std::sin(2 * M_PI * 440.0 * i / 44100.0);
        auto freq = [&](const std::vector<float>& v) { int zc = 0; const size_t n = v.size() / 2; for (size_t i = 1; i < n; i++) if ((v[i * 2] > 0) != (v[(i - 1) * 2] > 0)) zc++; return zc / 2.0 / (n / 44100.0); };
        auto fast = stretchPitch(sine, 44100, 2.0, 0), up = stretchPitch(sine, 44100, 1.0, 12), slow = stretchPitch(sine, 44100, 0.5, 0);
        CHECK(std::fabs(fast.size() / 2.0 - N / 2.0) < 4000 && std::fabs(freq(fast) - 440) < 15, "stretch 2x: %.2fs at %.0f Hz", fast.size() / 2.0 / 44100, freq(fast));
        CHECK(std::fabs(slow.size() / 2.0 - N * 2.0) < 6000 && std::fabs(freq(slow) - 440) < 15, "stretch 0.5x: %.2fs at %.0f Hz", slow.size() / 2.0 / 44100, freq(slow));
        CHECK(std::fabs(up.size() / 2.0 - N) < 100 && std::fabs(freq(up) - 880) < 20, "pitch +12: %.2fs at %.0f Hz", up.size() / 2.0 / 44100, freq(up));
        std::vector<float> hits((size_t)N * 2 * 2, 0.f);   // three clicks with decays
        for (int h : {2000, 30000, 60000}) for (int i = 0; i < 6000; i++) hits[(size_t)(h + i) * 2] = hits[(size_t)(h + i) * 2 + 1] = std::exp(-i / 800.f) * std::sin(i * 0.3f);
        auto sl = findSlices(hits, N * 2, 44100, 16);
        auto near = [&](int t) { for (int v : sl) if (std::abs(v - t) < 600) return true; return false; };
        CHECK(near(2000) && near(30000) && near(60000) && sl.size() <= 5, "slicer finds the three hits (%zu slices)", sl.size());
        auto eq = findSlices(std::vector<float>((size_t)N * 2, 0.1f), N, 44100, 16);
        CHECK(eq.size() == 16, "no hits: %zu equal slices", eq.size());
    }
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails ? 1 : 0;
}
