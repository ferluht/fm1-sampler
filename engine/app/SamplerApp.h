// SamplerApp: the whole sampler + audio looper on top of Session, and its FM-1 side.
// Bytes in (MIDI of the FM-1), bytes out (SysEx draw commands for its 240x240 screen and LEDs), no platform code: runs unchanged on the host
// (tools/app_sim) and in the Android app. The phone UI (ui/SamplerUI) reads the same knobs and edits them by touch.
//
// FM-1 map:  encoders  SELECT = track (1..8) | PRESETS = sample library with audition | ALGORITHM = sampler algorithm | K1..K4 = page knobs
//            buttons   HOME sample (start, end, speed, pitch) | FX | ENV | LFO | ARP | SEQ tape (loop speed, exciter, lo-fi, wobble) | EDIT mixer
//                      GLO key and scale | SAVE loop save (tap) / clear (hold) | PLAY | REC loop record / overdub
#pragma once
#include "../session/Session.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

struct SurfaceCell { std::string text; uint16_t fg = 0, bg = 0; bool known = false; };

class SamplerApp {
public:
    using Send = std::function<void(const uint8_t*, size_t)>;
    static constexpr int kTracks = 8;
    enum Button { OCT_DOWN, OCT_UP, FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC };
    enum Page { P_HOME, P_FX, P_ENV, P_LFO, P_ARP, P_SEQ, P_MIX, P_KEY, P_SAVE, P_LEN, kPages };
    struct Knob {
        std::string name, value;
        float norm = 0;
        bool valid = false, isChoice = false;
        std::function<void(int)> step;       // encoder detents
        std::function<void(float)> set;      // absolute 0..1 (touch)
    };
    using Knobs = std::array<Knob, 4>;
    struct Entry { std::string label, group, path; bool kit; };
    struct ScaleState { int scale = 3, root = 0, octave = 0, mode = 0; };

    SamplerApp(Session& s, Send send);

    void onMidi(const uint8_t* msg, size_t n);
    void onSysex(const uint8_t* msg, size_t n);
    void tick(uint32_t nowMs);                    // ~30 Hz

    // ---- state for the phone UI (all of it can also be edited from there) ----
    Page page() const { return page_; }
    void gotoPage(Page p);
    static const char* pageName(Page p);
    int track() const { return track_; }
    void selectTrack(int t);
    Knobs knobs();
    Knobs knobs(Page p);
    static const char* algoName(int i);
    int algo() const { return ts_[track_].algo; }
    void setAlgo(int a);
    const std::vector<Entry>& library() const { return lib_; }
    int libraryCursor() const { return libCursor_; }
    void browse(int delta);                       // PRESETS: moves the cursor, audition after a short pause
    void loadEntry(int index);                    // at once, on the selected track
    void refreshLibrary();
    void importFile(const std::string& wavPath);  // a file picked from the disk (already copied into the data dir as a WAV)
    bool wantFile() { bool w = wantFile_; wantFile_ = false; return w; }   // the UI asked for the disk picker: Java polls this
    void requestFile() { wantFile_ = true; }
    // looper and recorder
    void transportPlay();
    void loopRecord();                            // REC: arm / end the first take, or toggle overdub once a loop exists
    void lenAction(bool dbl);
    void loopSave();
    void loopClear();
    void micSample();                             // toggle: record the microphone into a new sample for the selected track
    float micThreshold() const { return 0.005f * std::pow(100.f, micThr_); }   // linear level that starts a take
    void setMicThreshold01(float v) { micThr_ = std::min(1.f, std::max(0.f, v)); s_.setMicThreshold(micThreshold()); }
    float micThreshold01() const { return micThr_; }
    int micState() const { return mic_; }         // 0 idle, 1 waiting for sound, 2 recording, 3 saving
    bool loopExists() const { return loopAny_; }
    std::string loopText() const;
    std::string toast() const { return nowMs_ < toastUntil_ ? toast_ : ""; }
    void say(const std::string& t) { toast_ = t; toastUntil_ = nowMs_ + 1800; }
    // mixer and per-track state
    struct TrackState {
        int algo = 0;
        float a = 0.1f, d = 0.35f, s = 0.8f, r = 0.4f;
        float lfoRate = 0.3f, lfoDepth = 0.f; int lfoTarget = 0, lfoShape = 0;
        float start = 0.f, end = 1.f, speed = 0.5f, pitch = 0.5f;
        int arpRate = 0, arpPatt = 0, arpOct = 0; float arpLen = 0.7f;
        
        float vol = 0.8f, pan = 0.5f, sendR = 0.5f, sendD = 0.2f;
        int entry = -1;
    };
    const TrackState& state(int t) const { return ts_[t]; }
    void setVolume(int t, float v); void setPan01(int t, float v); void setSend(int t, int which, float v);
    Session::Status status() const { return st_; }
    bool hasSample(int t) const { return !s_.trackLabel(t).empty(); }
    ScaleState scale() const { return scale_; }
    void setScale(int scale, int root, int octave, int mode);
    static const char* scaleName(int i);
    static const char* noteName(int r);
    void playNote(int note, bool down);
    void keyEvent(int rawNote, bool down);        // FM-1 key: chromatic note from the firmware
    bool linkSeen() const { return linkSeen_; }
    std::vector<float> peaks(int t, int bins);    // waveform of a track's sample, cached
    std::string sampleLabel(int t) const { return s_.trackLabel(t); }
    Session& session() { return s_; }

private:
    // ---- FM-1 drawing ----
    void sysex(int cmd, const std::vector<uint8_t>& payload, bool urgent = false, int key = -1);
    void clear(uint16_t c);
    void rect(int id, int x, int y, int w, int h, uint16_t c);
    void drawText(int id, int x, int y, int scale, uint16_t fg, uint16_t bg, const std::string& s, int width_chars);
    void draw(uint32_t now);
    void leds();
    void flush(uint32_t now);
    void heal();
    void hero(uint32_t now, const Knobs& k);
    // ---- input ----
    void button(int b, bool down);
    void encoder(int i, int delta);
    // ---- parameters ----
    void applyAll(int t);
    void applyAlgo(int t); void applyEnv(int t); void applyLfo(int t); void applyHome(int t, bool commit = false); void applyArp(int t); void applySeq(int t); void applyMix(int t);
    void applyFx();
    void queueCommit(int t);
    Knob cont(const char* name, float* v, std::function<std::string(float)> fmt, std::function<void()> apply, float stepSize = 0.02f, bool commit = false);
    Knob choice(const char* name, int* v, std::vector<std::string> opts, std::function<void()> apply);

    Session& s_;
    Send send_;
    Page page_ = P_HOME;
    int track_ = 0;
    std::array<TrackState, kTracks> ts_;
    // global effects
    float reverbSize_ = 0.7f, reverbDw_ = 0.35f, delayTime_ = 0.4f, delayFb_ = 0.45f;
    int loopSrc_ = 1;
    float exciter_ = 0.f, lofi_ = 0.f, wobble_ = 0.f, loopSpeed_ = 2.f / 3.f, micThr_ = 0.452f;   // loop speed is global
    uint32_t repaintAt_ = 0; int repaintCount_ = 0;
    // library
    std::vector<Entry> lib_;
    int libCursor_ = 0, loadedCursor_ = -1;
    uint32_t auditionAt_ = 0, previewOffAt_ = 0; int previewNote_ = -1, previewTrack_ = -1;
    bool wantFile_ = false, previewAfterLoad_ = false;
    // loop / recorder
    Session::Status st_{};
    bool loopAny_ = false;
    int mic_ = 0, micTrack_ = 0;
    uint32_t saveDownAt_ = 0; bool saveHeld_ = false, saveHoldDone_ = false;
    int lastSampleVersion_ = 0, lastSampleFails_ = 0;
    uint32_t commitAt_ = 0; int commitTrack_ = -1;
    std::string toast_; uint32_t toastUntil_ = 0;
    std::array<std::vector<float>, kTracks> peaks_; std::array<uint32_t, kTracks> peaksAt_{}; std::array<std::string, kTracks> peaksLabel_;
    // FM-1 link
    struct Frame { int key; std::vector<uint8_t> bytes; };
    std::deque<Frame> pending_; size_t pendingBytes_ = 0;
    uint32_t gap_ = 16, lastSend_ = 0, lastPing_ = 0, lastLoss_ = 0, lastRaise_ = 0, dropped_ = 0, healAt_ = 0, lastDraw_ = 0, nowMs_ = 0;
    bool haveDropBase_ = false, needClear_ = true, needResend_ = false, linkSeen_ = false;
    int pingId_ = 0, healCursor_ = 0;
    uint64_t lastLed_ = ~0ull;
    int knobMsb_ = 0;
    int heldNote_[128];
    int keyNotes_[128][3] = {}; int keyCount_[128] = {};
    bool fwDirty_ = true; uint32_t lenAt_ = 0;
    uint32_t held_ = 0;
    ScaleState scale_;
    uint32_t touchedAt_ = 0; int touchedKnob_ = -1;
    std::vector<SurfaceCell> cells_;
    std::vector<uint64_t> rects_;
};
