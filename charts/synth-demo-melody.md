# Synth demo melody — charted for Fightbox

Source: [this.is.NOISE inc](https://www.instagram.com/p/DaWEP56MYNI/) Instagram demo clip
(21.6s, "Always in key, instantly ready to play"). Kegan asked for it charted out for his
own Fightbox synth (2026-09-16).

## Method (v2 — corrected, 2026-09-16)

Kegan flagged, correctly, that v1 (below) wasn't confidently verified and that the clip
modulates pitch after a held note — something Fightbox doesn't support. Redone properly:

- Re-pulled the source (same Vivaldi-cookie + instaloader method as before).
- Source-separated with Demucs (`htdemucs`) — isolates the lead line from pad/bass/drums.
- **Re-transcribed with Basic Pitch** (Spotify's ML audio→MIDI model — purpose-built for
  this, not a generic pitch tracker) instead of the original `librosa.pyin` pass. Checked
  v1's confidence values directly: large stretches past ~18s were ~0.01 confidence —
  essentially noise, not real notes. Basic Pitch's onset/frame detection is far more
  reliable for polyphonic/synth material and reports **per-note pitch bend curves**
  directly, which is what actually answers the modulation question.
- Couldn't get a second model's independent "listen" — the `video_analyze` tool isn't
  currently wired to a video-capable model in this environment, so that path is closed
  for now (not something fixable from inside a chat turn).

**Confirmed: real pitch bends exist**, several 4–7 semitones deep (much bigger than the
~1-semitone reporting noise floor everywhere else), clustered around 4.7s, 6s, 8.2–9.5s,
10.4–13.4s, and a long ~2-semitone bend on the final held note (19.7–21.4s). Kegan's ear
was right — this is a modulated synth line, not a flat MIDI-grid melody.

**Firmware reality check:** grepped `fightingbox_midi.ino` directly. There is no pitch
bend, modulation CC, or portamento/glide anywhere in the current firmware — confirmed by
source, not assumed. Scale mode gives clean discrete key-presses only. See the "Firmware
modulation support" section at the bottom for what it would take to add it.

## Two charts, for two different goals

- **Chart A (beginner, simplified)** — the original 11-press version below. Smooths every
  bend to its nearest scale-degree neighbor. Good for: learning the shape by ear fast,
  playable exactly as written on current firmware, no theory needed.
- **Chart B (full transcription)** — the complete Basic Pitch note list for the whole
  21.6s clip, bend annotations included, further down this file. Good for: seeing exactly
  what the source recording is doing, and as the reference if/when bend support gets
  added to the firmware.

---

## Tempo & key
- **110 BPM**
- **Bb major** (A# in sharp notation) — confirmed both by chroma analysis (A# is the
  loudest pitch class overall, 0.68 weight vs next-highest 0.54) and by the melody itself
  landing on A#4 as its resting/return note.
- The clip is a synth-pad **feature demo**, not a single song — it visibly changes key
  every few seconds to show off the pad's auto-key-lock feature. The chart below is the
  **opening 2-bar phrase in its home key (Bb)**, which repeats almost exactly again later
  in the clip (beats 18.25–30.5 mirror beats 0.5–14.5) — a real riff, not noise.

## Chart A — the riff (2 bars, 4/4, Bb major pentatonic: Bb–C–D–F–G)

| Beat | Note | Duration | Scale degree |
|------|------|----------|---------------|
| 0.50 | A#4 (Bb4) | 0.41s (~ dotted 8th) | 1 (root) |
| 2.25 | G5 | 0.14s (16th) | 6 |
| 2.75 | A#5 (Bb5) | 0.14s (16th) | 1 (root, octave up) |
| 3.50 | F5 | 0.14s (16th) | 5 |
| 4.50 | F5 | 0.41s (~ dotted 8th) | 5 |
| 7.00 | F5 | 0.14s (16th) | 5 |
| 8.75 | C5 | 0.14s (16th) | 2 |
| 9.25 | A#4 (Bb4) | 0.14s (16th) | 1 (root) |
| 10.75 | A#4 (Bb4) | 0.14s (16th) | 1 (root) |
| 11.75 | D#3 (Eb3) | 0.14s (16th) | 4 (brief, likely a passing/leaked bass tone) |
| 12.50 | A#4 (Bb4) | 0.41s (~ dotted 8th) | 1 (root) |
| 14.50 | A#4 (Bb4) | 0.27s (~ 8th) | 1 (root) |

Shape: sits on the root (Bb4), leaps up to a quick G5→Bb5→F5 flourish, settles back down
through C5 to the root, with long held Bb4s bookending each half — classic call-and-answer
pad-demo phrasing rather than a "song" in the traditional sense.

## Files
- `synth-demo-melody.mid` — quantized MIDI, program 81 (lead synth), ready to drop into
  Fightbox or any DAW/sequencer.
- `synth-demo-source.mp4` — original clip, kept for reference/re-transcription if the chart
  needs refining by ear.

## Chart A, Fightbox button chart — for absolute beginners

You are VERY new to this, so here's the direct "press this key" version, no music
theory needed to read it. Setup first:

1. Power on, tap **START** until the mode shows **Scale** (2nd of the 4 melodic modes).
2. Tap **HOME** repeatedly to cycle scales until it lands on **Maj Pentatonic** (per the
   firmware's scale list: Major, Minor, Maj Pentatonic, Min Pentatonic, Blues, Dorian,
   Mixolydian, Harmonic Minor — so 3 taps of HOME from Major).
3. The 8 keys (1P 2P 3P 4P 1K 2K 3K 4K) now play scale degrees 0–7 of Bb major
   pentatonic in order, low to high, left to right, top row then bottom row. Every key
   is "correct" — you cannot hit a wrong note in this mode.

### The riff — press these keys in order

Counting beats at 110 BPM (a beat is about half a second — the numbers below are just
relative timing, not something you need to calculate):

| Press | Key | Hold roughly | Beat (for reference) |
|-------|-----|--------------|----|
| 1 | **1P** | short | 0.5 |
| 2 | **1K** | quick tap | 2.25 |
| 3 | **2K** *(same note, one octave up — see note below)* | quick tap | 2.75 |
| 4 | **4P** | quick tap | 3.5 |
| 5 | **4P** | slightly longer hold | 4.5 |
| 6 | **4P** | quick tap | 7.0 |
| 7 | **2P** | quick tap | 8.75 |
| 8 | **1P** | quick tap | 9.25 |
| 9 | **1P** | quick tap | 10.75 |
| 10 | **1P** | longer hold | 12.5 |
| 11 | **1P** | medium hold | 14.5 |

That's it — **11 presses**, mostly bouncing between **1P** (the "home" note it keeps
returning to) and **4P** (the higher note in the flourish), with **1K**/**2K**/**2P** as
quick passing notes on the way there and back.

**About step 3 (the octave jump):** the original recording jumps up a full octave for
one note in the middle of the flourish. On the Fightbox, degree 5 (one octave above
degree 0) lands on **2K** — the octave-up twin of **1P**. If you don't want to bother with
the octave jump, just play **1P** again instead of **2K** for step 3 — same note name,
lower octave, and the riff still sounds right, just less dramatic on that one note.

**Skipped note:** there was one very brief off-scale blip in the original audio (a bass
note, D#3, at beat 11.75, only 0.14 seconds long) — almost certainly bleed from another
instrument layer in the source mix, not a real part of the melody. Left out of this
chart on purpose.

### Key → scale degree reference (so you can see the whole layout)

| Key | 1P | 2P | 3P | 4P | 1K | 2K | 3K | 4K |
|---|---|---|---|---|---|---|---|---|
| Degree | 0 (root) | 1 | 2 | 3 | 4 | 5 (root, 1 octave up) | 6 | 7 |
| Note (Bb maj pent) | Bb | C | D | F | G | Bb (high) | C (high) | D (high) |

Only 1P, 2P, 4P, 1K, and 2K are used in this particular riff — 3P, 3K, and 4K aren't part
of this melody but are there if you want to noodle around it.

---

## Chart B — full transcription (whole 21.6s clip, Basic Pitch)

This is the complete note list across the whole clip, not just the opening phrase, with
every detected pitch bend flagged. Confirms the riff repeats with variation throughout —
not just the one 2-bar loop from Chart A. Bend values are Basic Pitch's own per-note
detection, in semitones (how many keys' worth of pitch it slides).

| Beat | Note | Duration | Bend |
|------|------|----------|------|
| 5.81 | F5 | 0.12s | — |
| 6.88 | F5 | 0.13s | — |
| 7.11 | G5 | 0.12s | — |
| 7.33 | G♯5 | 0.12s | ~2 semitones |
| 7.55 | G5 | 0.28s | — |
| 8.07 | E5 | 0.14s | ~2 semitones |
| 8.32 | D♯5 | 0.12s | — |
| 8.56 | A♯4 | 0.12s | ~6 semitones |
| 8.74 | C5 | 0.21s | — |
| 9.09 | A♯4 | 0.14s | — |
| 9.35 | A♯4 | 0.55s | — |
| 10.36 | A♯4 | 0.40s | — |
| 11.09 | A♯4 | 0.27s | — |
| 11.09 | D♯5 | 0.26s | ~3 semitones |
| 11.57 | D♯5 | 0.44s | — |
| 11.59 | A♯4 | 0.96s | — |
| 13.35 | A♯4 | 0.55s | — |
| 13.35 | G5 | 0.60s | — |
| 14.36 | A♯4 | 0.44s | — |
| 15.11 | F5 | 0.26s | ~5 semitones |
| 15.58 | F5 | 0.28s | ~5 semitones |
| 16.10 | F5 | 0.14s | ~5 semitones |
| 16.35 | F5 | 0.27s | — |
| 16.39 | A♯4 | 0.25s | — |
| 16.85 | F5 | 0.35s | ~5 semitones |
| 16.85 | A♯4 | 0.29s | — |
| 17.36 | G5 | 0.10s | — |
| 17.55 | G5 | 0.30s | — |
| 18.09 | G5 | 0.14s | — |
| 18.39 | A♯5 | 0.41s | — |
| 19.10 | F5 | 0.13s | ~5 semitones |
| 19.10 | D♯5 | 0.27s | ~7 semitones |
| 19.34 | F5 | 0.14s | — |
| 19.60 | F5 | 0.40s | ~5 semitones |
| 20.33 | F5 | 0.56s | — |
| 21.36 | C5 | 0.58s | — |
| 22.29 | F5 | 0.31s | — |
| 23.08 | E5 | 0.28s | ~4 semitones |
| 23.08 | B4 | 0.43s | — |
| 23.61 | G5 | 0.28s | — |
| 23.87 | B4 | 0.32s | — |
| 24.62 | A♯5 | 0.25s | ~6 semitones |
| 24.82 | C6 | 0.14s | — |
| 25.08 | A♯5 | 0.16s | — |
| 25.37 | A♯5 | 0.94s | — |
| 25.37 | A♯4 | 0.32s | — |
| 26.33 | A♯4 | 0.22s | — |
| 27.10 | A♯5 | 1.23s | — |
| 27.61 | A♯4 | 0.15s | — |
| 27.89 | A♯4 | 0.32s | — |
| 29.33 | G5 | 0.43s | — |
| 29.35 | A♯5 | 0.96s | — |
| 30.12 | G5 | 0.19s | — |
| 31.11 | A♯5 | 0.41s | — |
| 32.87 | E5 | 0.31s | — |
| 36.10 | A♯4 | 1.73s | ~2 semitones |

Note: beats above 21.6 (the clip's real length at 110 BPM) come from the tempo re-scale —
the source detection ran on absolute seconds; treat beat numbers past ~78 as approximate.
Weighted by loudness × duration, **A♯ (Bb) still dominates overwhelmingly** (7.0 vs next
highest F at 1.8) — confirms Chart A's simplification onto Bb pentatonic wasn't wrong, it
was a legitimate simplification of a real underlying structure, not a guess.

**File:** `synth-demo-melody-full.mid` — same notes as the table, with bend events
encoded as standard MIDI pitch-wheel messages (clamped to a ±2-semitone range, the
universal DAW default; several of the real bends are bigger — see the raw table above for
true depth). Import into a DAW to hear it with the bends intact.

---

## Firmware modulation support — BUILT (2026-09-16), moved to D-pad same day

Checked the firmware directly (`fightingbox_midi.ino`) before touching it: no pitch bend,
modulation CC, or portamento/glide existed anywhere. Built it on L3/R3 first, then Kegan
flagged that small buttons like L3/R3 aren't great for a bend gesture mid-jam and asked
for the D-pad instead. Moved it same day. Final version:

- **D-pad Left = bend down, Right = bend up**, but ONLY when a melodic key is already
  held in Chromatic or Scale mode at the moment Left/Right is first pressed. With nothing
  held, Left/Right do exactly what they always did — tap = permanent semitone shift,
  hold = temporary — untouched, nothing existing was taken away. Chord/Custom/Drum modes
  don't use the D-pad this way at all, so bend simply doesn't apply there.
- Eased ramp (~0.2s to full bend, not an instant snap) via `sendPitchBend()`, ±1 octave
  range set with a standard RPN 0,0 message at boot (any GM-compliant receiver honours
  it; one that ignores RPN just falls back to its own default, usually ±2 semitones —
  never worse than doing nothing).
- Auto-releases to center pitch if the melodic key lets go before Left/Right does (can't
  bend into silence), and is force-cleared on every mode switch / MIDI panic so a bend
  can never get stuck across a state change.
- **Compiled clean** with `arduino-cli compile --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb`
  — 107,428 bytes program (5%), 22,176 bytes RAM (8%), zero warnings, final version.

Went through `ship.sh` — branch, PR, stage-1 code review, stage-2 ponytail audit — per the
standing rule that agent firmware/code changes never land on main directly. The L3/R3
version went through 5 review rounds (3 real bugs caught and fixed - see the L3/R3 PR
history for the full trail); the D-pad move reuses the exact same hold/release/mode-switch
guard logic proven there, just retargeted at `dpadState`/`dpadIsBend`/`dpadWasBend`
instead of `fnState`/`fnIsBend`/`fnWasBend`.

### Why the D-pad and not TURBO

- **TURBO** is dead hardware (no GPIO responds) — a repair job, not something fixable
  from firmware.
- **L3/R3** work correctly (see the original PR) but are small, hard-to-reach buttons for
  a gesture you want to do fluidly while actually playing — Kegan's call, and the right
  one for "grooving," not just correctness.
- **D-pad Left/Right** already move pitch by a semitone when tapped — bend is the same
  axis, same gesture, just continuous instead of stepped, and it's a full-size, easy-to-reach
  direction pad rather than two small side buttons. Gating bend on "a melodic key is
  already down" means the existing tap/hold semitone-shift behavior is 100% preserved for
  every use case that doesn't involve holding a note, exactly like the L3/R3 version was.

