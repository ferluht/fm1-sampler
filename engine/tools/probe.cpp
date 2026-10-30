// Probe: what the default project loads and how long it takes
#include <Session.h>
#include <SamplerApp.h>
#include <Common.h>
#include <chrono>
#include <thread>
#include <cstdio>
int main() {
    Session s(SAMPLERATE); s.setDataDir("/tmp/fm1sampler_t3"); s.setSampleDir(std::string(getenv("HOME")) + "/fm1-sampler/samples");
    auto t0 = std::chrono::steady_clock::now();
    SamplerApp app(s, [](const uint8_t*, size_t) {});
    std::vector<float> buf(512), in(512, 0.f); uint32_t now = 0;
    for (int i = 0; i < 400; i++) {
        std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); if (i % 5 == 0) { now += 33; app.tick(now); }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); if (i % 40 == 0) { printf("%.2fs:", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()); for (int t = 0; t < 5; t++) printf(" [%s]", app.sampleLabel(t).c_str()); printf(" fails=%d\n", s.status().sampleFails); }
    }
    return 0;
}
