# FM1 looper workflow (experimental, host-simulated only)

* Hold **REC** (~0.6 s, main page, transport stopped) -> header shows `ARM`.
* The first key you play starts the take (`LOOP`). No click needed.
* **REC** or **PLAY** closes the take: loop length becomes 16/32/64 steps and BPM is
  set so the loop is exactly as long as the take. Notes are quantised into the track,
  with gate lengths, chords on one step are kept (up to 8 notes). Playback restarts
  at step 1 and REC stays on, so everything you play next is overdubbed.
* **REC** toggles overdub. **HOME+SAVE** undoes the whole take (stop REC first).
* Any other button during ARM/LOOP cancels. Works on the selected NORMAL track,
  melodic or drum.

Code: `app/looper.inc` (included by `app/screen.c`). Test harness: build the web
sources natively (see tools/sim.py) and script the controls.
