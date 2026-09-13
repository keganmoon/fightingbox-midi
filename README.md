# FightingBox MIDI

Turns a FightingBox v1.0 (RP2040, originally running GP2040-CE) into a
class-compliant USB-MIDI instrument. No drivers, no host software: it
enumerates as `Pico MIDI` on Linux/macOS/Windows and as a Core MIDI device
on iOS via a USB camera adapter.

Ships with a browser-based live monitor that mirrors the controller's state
and labels every key with what it currently does.

```
projects/fightingbox_midi/
├── fightingbox_midi.ino   firmware
├── monitor.html           live monitor (Web MIDI, Chromium browsers)
└── README.md
projects/pin_probe/        diagnostic: sends GPIO number as note number
projects/i2c_probe_midi/   diagnostic: scans both I2C buses, reports via MIDI
```

---

## Hardware map (measured, not assumed)

Both the main keys and the function row were originally guessed from the
GP2040-CE backup using Hit Box naming conventions. **Both guesses were
wrong.** These values are measured.

| Key | GPIO | | Key | GPIO |
|---|---|---|---|---|
| 1P | 7  | | LEFT   | 0 |
| 2P | 8  | | UP     | 1 |
| 3P | 9  | | DOWN   | 2 |
| 4P | 10 | | RIGHT  | 3 |
| 1K | 11 | | HOME   | 4 |
| 2K | 12 | | SELECT | 5 |
| 3K | 13 | | START  | 6 |
| 4K | 14 | | L3     | 21 |
|    |    | | R3     | 22 |

**TURBO is dead.** No GPIO responds to it — most likely a disconnected
spade terminal or a failed switch, not a firmware problem. GPIO 20 is
unused. Everything is active-low with internal pull-ups.

If the Turbo switch is ever repaired, it frees a key and the M7 chord
extension can move back onto it (see *Known gaps*).

---

## Controls

### Modes

`START` taps through the four melodic modes. `R3` jumps to drums and back.

| Mode | The 8 keys play |
|---|---|
| **Chromatic** | 8 consecutive semitones from C4, channel 1 |
| **Scale** | 8 degrees of the selected scale — no wrong notes |
| **Chord** | keys pick the root, modifiers pick the quality |
| **Custom** | 4 editable banks, each key its own sound + own channel |
| **Drum (GM)** | real percussion on channel 10 |

### Function row

| Key | Tap | Hold (0.6 s) |
|---|---|---|
| ~~TURBO~~ | *dead switch* | — |
| **HOME** | next scale / chord-mode: +9th | **record** |
| **SELECT** | panic | shift layer + on-screen button map |
| **START** | next melodic mode | latch (sustain) mode |
| **L3** | looper transport / chord-mode: +6th | erase loop |
| **R3** | drums ⇄ melody / chord-mode: +m7 | — |

`SELECT` + `HOME` wipes all recorded overrides.

### D-pad — depends on mode

| Mode | Up / Down | Left / Right |
|---|---|---|
| Chromatic, Scale | octave ±1 | semitone ±1 |
| Chord | Major / Minor | Sus / Dim |
| Custom | octave ±1 | previous / next bank |
| Drum GM | drum bank | GM kit (Program Change) |
| *shift layer* | velocity ±10 | mute loop / clear loop |

In Chromatic and Scale, **tap = permanent** shift, **hold = temporary**
(snaps back on release). Holding two at once stacks both offsets.

### Chord mode

Modelled on the Telepathic Instruments Orchid: the keys choose the root,
the modifiers choose the quality.

- **Types** (d-pad): Major, Minor, Sus, Dim. Priority Maj > Min > Sus > Dim.
  With **no type active** you get the diatonic chord for that scale degree,
  i.e. the chord that belongs in the key.
- **Extensions** (L3 / R3 / HOME): +6th, +m7, +9th. Stack freely with each
  other and with any type.
- Every modifier is **hold = momentary, tap = latch**. Lock "Minor" on for
  a progression, or grab an extension for a single chord. `SELECT` clears
  every latch.

Scales: Major, Minor, Maj Pentatonic, Min Pentatonic, Blues, Dorian,
Mixolydian, Harmonic Minor. Scale choice and chord choice compose — the
diatonic shapes follow whichever scale is selected.

### Custom banks

Four banks of eight slots. Each slot holds 1–4 notes **plus its own MIDI
channel**, which is what lets one bank mix ch10 drums with pitched ch1
material.

| Bank | Default contents |
|---|---|
| Beat | Kick, Snare, Cl Hat, Op Hat, Clap, Stick, Ride, Crash (ch10) |
| Bass | C minor pentatonic walk, low register (ch1) |
| Stabs | eight C-minor triads that comp together (ch1) |
| Perc | tambourine, cowbell, maraca, guiro, congas, claves (ch10) |

**Editing:** hold `HOME` → press a source key → press a target key. The
write lands in the *currently active* bank's slot, and the arm state
survives bank changes — so you can capture a kick in Beat, swap to Bass,
and drop it into a slot there. The channel travels with the sound, so that
kick still plays as a real drum inside the bass bank.

Pitched slots follow octave/transpose; ch10 percussion slots deliberately
do not, since transposing GM percussion just selects a different instrument.

### Looper (L3)

Free-timing, single layer, unlimited overdubs.

```
idle --tap--> armed --(first note starts recording)
     --tap--> loop closes and plays
     --tap--> overdub          --tap--> back to play
     --hold-> erase
```

Recording starts on your **first note**, not on the button press, so there
is no dead air at the top of the loop. Overdubbed events are insertion-
sorted into the existing pass, so they interleave correctly on the next lap.
Capacity 320 events. `SELECT` + Left mutes, `SELECT` + Right clears.

### Latch mode (hold START)

Keys toggle instead of gate: press to start a note, press again to stop.
Turns eight momentary keys into eight sustaining voices you can stack by
hand — useful for drones and pads. Leaving latch mode releases everything.

### Recording sounds onto keys

Outside Custom mode this writes a **global override**: that key plays the
saved notes+channel in *every* mode until wiped. Park a real ch10 kick on
one key and keep playing melody on the rest.

Inside Custom mode the same flow writes into the active bank's slot instead.

---

## The monitor page

`monitor.html` — open in a Chromium browser (Chrome / Edge / Brave /
Vivaldi) and allow the MIDI permission. **Firefox has no Web MIDI.**

It draws the physical board and relabels every key for the current mode:
note names computed from your live octave and transpose, drum names in drum
modes, "pick as source/target" during recording. Keys light white while
physically held. Colour coding: green = held, amber = latched, blue =
recorded override, pink = drum voice, purple = pitched note, dashed = dead
switch.

### Running it

It is a single self-contained file — no server, no build step, no
dependencies. Open it directly:

```bash
xdg-open ~/projects/fightingbox_midi/monitor.html
```

Allow the MIDI permission when prompted. It connects to any input whose
name matches `pico` or `fighting`, and reconnects automatically when the
board is replugged, so you can leave the tab open across reflashes.

Deliberately **not** served from the board. Embedding a web server would
mean USB networking (RNDIS/ECM), which needs driver coaxing on Windows,
is not native on macOS, and does not work on iOS at all — a lot of
plumbing for something a local file already does, on a page you mostly
stop needing once the OLED shows the button map.

### How it knows

The firmware broadcasts its entire state as Control Change messages on
**MIDI channel 16**, chosen because nothing musical is sent there — a DAW
track listening to ch1/ch10 never sees this traffic. State is published on
every change plus a 1 Hz heartbeat, so a monitor opened late still syncs.

| CC | Meaning | | CC | Meaning |
|---|---|---|---|---|
| 20 | mode | | 32 | pressed mask, 1P–4P |
| 21 | scale index | | 33 | pressed mask, 1K–4K |
| 22 | active chord types (bitmask) | | 34 | pressed mask, d-pad |
| 23 | active extensions (bitmask) | | 35 | pressed mask, function row |
| 24 | octave + 64 | | 36 | override mask, 1K–4K |
| 25 | transpose + 64 | | 37 | loop state |
| 26 | GM kit index | | 38 | loop muted |
| 27 | drum bank index | | 39 | latch mode |
| 28 | record state | | 40 | velocity |
| 29 | override mask, 1P–4P | | 41 | shift layer held |
| 30 | latched types (bitmask) | | 42 | custom bank index |
| 31 | latched extensions (bitmask) | | | |

CC values are 7-bit, so any 8-wide bitmask is **split across two CCs**.
Packing eight buttons into one CC silently drops the 4K bit.

---

## Building and flashing

```bash
# one-time setup
arduino-cli config add board_manager.additional_urls \
  https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli core install rp2040:rp2040
arduino-cli lib install "Adafruit TinyUSB Library" "MIDI Library"

# build + flash (hold BOOTSEL while plugging in first)
cd ~/projects/fightingbox_midi
arduino-cli compile --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb .
udisksctl mount -b /dev/sdb1          # if it doesn't auto-mount
arduino-cli upload  --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb -p UF2_Board .
```

The `:usbstack=tinyusb` suffix is required — the default USB stack has no
MIDI class. The sketch folder name must match the `.ino` filename.

Verify with `amidi -l` (expect `Pico MIDI 1`) and watch traffic with
`aseqdump -p <client>:0`.

### Restoring the fightstick

Nothing here is permanent. Hold BOOTSEL, drag the original GP2040-CE `.uf2`
onto the `RPI-RP2` drive, then re-import `gp2040ce_backup_*.gp2040` through
the web configurator (hold **START** while plugging in, browse to
`http://192.168.7.1`).

---

## Known gaps

- **No M7 chord extension** — it lived on the dead Turbo key.
- **Velocity is uniform.** The keys are digital, so there is no touch
  sensitivity; the shift layer sets a global velocity instead.
- The looper is free-timing only. No tempo, click, or quantise.

## On-board display

A 128x64 SSD1306 OLED at `0x3C` on **I2C block 1** (GPIO 26 SDA / 27 SCL),
driven through `Wire1` — *not* `Wire`, which is block 0 and physically
cannot reach those pins. See `DISPLAY.md`.

**Status view** (default): mode in large text, the setting that matters for
that mode (scale / chord spelling / bank / kit), octave-transpose-velocity,
anything currently engaged (loop, latch, record prompts), and which keys
hold a recorded sound. A brief `SAVE` appears when settings commit.

**Button map** (hold `SELECT`): draws the physical board — d-pad staggered
lower-left, punches and kicks in two rows on the right — with every key
labelled by what it does *in the current mode*. Note names in
Chromatic/Scale, degree numbers in Chord, `BD SD CH OH…` in drum banks,
`SAV` on any key holding a recorded sound. **Keys fill solid while held**,
so it doubles as an input tester.

Both views redraw at ~12 fps so the screen never delays note output.

Layout note: at this size a 16px box fits two 6px glyphs, and the footer
starts at x=52 so it holds 12 characters. Changes to the labels are easy to
overflow — mock the geometry and look at it before flashing.

## Persistence

Banks, recorded overrides, scale, kit, octave, transpose, velocity and the
current mode are saved to flash and restored on power-up. There is no save
button.

Flash writes stall the CPU for milliseconds, so nothing is written from the
hot path: changes set a dirty flag and commit after ~2.5 s of quiet, never
while a key is held or the looper is recording.

**`SELECT` + `START` = factory reset** — wipes saved settings and restores
the compiled-in banks. A two-key chord so it cannot be hit by accident.

## Troubleshooting

**Enumeration failures** (`device descriptor read/64, error -110`, repeated
disconnects) were traced to USB hubs and marginal cables, not firmware —
a bare-bones sketch with no MIDI or display code failed identically. Plug
directly into the machine and avoid hubs. If a port wedges after many
BOOTSEL cycles, reboot to reset the xHCI controller.

**BOOTSEL mode is ROM-level** and works regardless of what firmware is
installed, so a bad flash can always be recovered.
