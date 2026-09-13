# The display — solved

**The OLED works. The bug was mine: wrong I²C bus object.**

## Root cause

From `gp2040ce_backup_ORIGINAL.gp2040`:

```json
"display": { "enabled": 1, "sdaPin": 26, "sclPin": 27,
             "i2cAddress": "0x3c", "i2cBlock": 1, "i2cSpeed": 400000 }
```

`"i2cBlock": 1` was the field that mattered.

On the RP2040 each GPIO can only reach one specific I²C block, and **GPIO
26/27 belong to block 1 — block 0 physically cannot drive them.** The
Arduino-Pico core exposes block 0 as `Wire` and block 1 as `Wire1`.

The firmware called:

```cpp
Wire.setSDA(26);   // block 0, on pins only block 1 can reach
```

This fails silently. Nothing ever ACKs, the presence probe finds no device,
and the code correctly concludes "no display present" and skips drawing.
A wrong-bus setup is indistinguishable from a dead panel from the outside:
silence either way.

Fix: `Wire1` throughout, and construct the driver against it —
`Adafruit_SSD1306 display(W, H, &Wire1, -1)`.

## Proof

`projects/i2c_probe_midi` scans both buses and reports hits as MIDI notes
(serial was unreliable on this machine; MIDI capture never was). Address
becomes the note number, bus becomes the channel.

Result, five consecutive passes:

```
ch1  note 60   -> 0x3C found on Wire1 (block 1, GPIO 26/27)
ch16 note 1    -> heartbeat
(nothing on ch2 -> block 0 empty, as expected)
```

0x3C on block 1 is exactly what the config file specified all along.

## Theories that were wrong

- **SH1106 instead of SSD1306.** Plausible, and common on cheap GP2040-CE
  boards, but wrong here — a stock `SSD1306` init works fine.
- **The button pinout errors.** Real bugs, but unrelated: buttons are on
  GPIO 0–14 and 20–22, the display on 26/27. Different pins, different
  peripheral.
- **The dead Turbo switch.** An open circuit on one input pin has no path
  to the I²C bus.

They do share a *cause* with the display bug: a value guessed from
convention when the correct one was sitting in the backup JSON. `i2cBlock`,
like the real pin numbers, was in that file the whole time.

## A genuine second bug, also fixed

The original display code called `display.begin()` **before** USB
enumeration finished. On a bus where nothing answers, that call can block
long enough to stall the host handshake, making the entire device
undetectable — this produced `device descriptor read/64, error -110` and a
board that appeared bricked.

Current code: bring up USB first, then probe for an ACK with a bounded
timeout, and only call `begin()` if something answers. A missing or broken
panel can no longer prevent the instrument from working.

## Note on testing with stock GP2040-CE

Reflashing stock GP2040-CE as a hardware sanity check **does not work on
this board**, worth recording so nobody retries it:

- The stock Pico build ships with the display **disabled by default**, so a
  blank screen there would prove nothing without importing the config JSON
  first.
- Importing requires the web configurator, which needs **S2 = GPIO 17** in
  the stock default pinout. Nothing on this board is wired to GPIO 17.
- Holding this board's Start (GPIO 6) at boot instead matches stock `B1`,
  which selects Nintendo Switch mode — the board enumerates as a "HORI
  Pokken" controller rather than opening web config.

`http://192.168.7.1` only exists while GP2040-CE is flashed; it is a
GP2040-CE feature (embedded HTTP server over USB networking), not something
this firmware provides. It would not have identified the panel regardless —
it exposes wiring config, not the controller chip.

## What the screen shows

Refreshed at ~12 fps, deliberately throttled to keep MIDI latency low:

```
CHROMA              <- mode, large
Major               <- scale / chord quality / bank / kit, per mode
oct +0  semi +0  v100
LOOP play LATCH     <- whatever is engaged right now
saved: 1P 3K        <- keys holding a recorded sound
```

In Chord mode line 2 shows the live chord spelling, e.g. `Minor +m7`, or
`diatonic` when no modifier is held.
