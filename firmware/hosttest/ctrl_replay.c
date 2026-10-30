/* Host test of app/ctrl.c: stubs for the FM-1 SDK, scripted panel input, captured MIDI/SysEx and a framebuffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm1.h"
#include "ctrl.c"

/* ---- SDK stubs ---- */
static struct fm1_controls ctl;
static uint32_t ms;
static uint16_t fb[240 * 240];
static unsigned char midi_log[4096][3]; static int midi_n; static int midi_full;
static unsigned char sx_log[4096][40]; static int sx_len[4096]; static int sx_n;
void fm1_controls_init(void) {}
void fm1_controls_read(struct fm1_controls *s) { *s = ctl; memset(ctl.encoders, 0, sizeof ctl.encoders); }
void fm1_led_set(unsigned k, int on) { (void)k; (void)on; }
uint32_t fm1_millis(void) { return ms; }
void fm1_control_request(void) {}
void fm1_screen_init(void) {}
void fm1_screen_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { for (unsigned j = y; j < y + h && j < 240; j++) for (unsigned i = x; i < x + w && i < 240; i++) fb[j * 240 + i] = c; }
int fm1_screen_blit(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *p, size_t n) { (void)n; for (unsigned j = 0; j < h; j++) for (unsigned i = 0; i < w; i++) if (x + i < 240 && y + j < 240) fb[(y + j) * 240 + x + i] = p[j * w + i]; return 0; }
int fm1_midi_send_port(unsigned port, const uint8_t *m, size_t n) {
    (void)port;
    if (midi_full) return -2;
    if (m[0] == 0xf0) { sx_len[sx_n] = (int)n; memcpy(sx_log[sx_n], m, n > 40 ? 40 : n); if (sx_n < 4095) sx_n++; return 0; }
    midi_log[midi_n][0] = m[0]; midi_log[midi_n][1] = m[1]; midi_log[midi_n][2] = m[2]; midi_n++; return 0;
}


int groove_master_update(unsigned knob) { (void)knob; return 0; }
static void ppm(const char *p) {
    FILE *f = fopen(p, "wb"); fprintf(f, "P6\n240 240\n255\n");
    for (int i = 0; i < 240 * 240; i++) { uint16_t c = fb[i]; unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
    fclose(f);
}
/* Replays the frames the phone engine produced (surface_sim FRAMES=...) through the firmware's own rx queue and drawing code. */
int main(int argc, char **argv) {
    groove_screen_init();
    FILE *f = fopen(argc > 1 ? argv[1] : "/tmp/fm1_frames.bin", "rb");
    if (!f) { puts("no frames file"); return 2; }
    int frames = 0, perTick = argc > 2 ? atoi(argv[2]) : 1;
    for (;;) {
        uint16_t l; if (fread(&l, 2, 1, f) != 1) break;
        static uint8_t m[1024]; if (fread(m, 1, l, f) != l) break;
        if (!ctrl_sysex(m, l)) printf("frame %d (cmd %u, %u bytes) was not taken by ctrl_sysex\n", frames, m[6], l);
        frames++;
        if (frames % perTick == 0) { ms += 16; groove_screen_control(); }
    }
    for (int i = 0; i < 8; i++) { ms += 16; groove_screen_control(); }
    printf("%d frames: rx_frames %u rx_dropped %u\n", frames, rx_frames, rx_dropped);
    unsigned black = 0; for (int i = 0; i < 240 * 240; i++) black += fb[i] == 0;
    printf("black pixels %u of %u\n", black, 240u * 240u);
    ppm("replay_fw.ppm");
    return 0;
}
