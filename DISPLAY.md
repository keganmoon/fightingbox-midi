# The display problem

The board has an SSD1306-style 128×64 I²C OLED that GP2040-CE drove fine.
Under this firmware it stays blank. This is what we know, what we ruled
out, and what to try next.

## What the backup says

From `gp2040ce_backup_20260911152301079.gp2040`:

```json
"display": { "enabled": 1, "sdaPin": 26, "sclPin": 27,
             "i2cAddress": "0x3c", "i2cBlock": 1, "i2cSpeed": 400000 }
```

## The leading suspect: wrong I²C bus object

`"i2cBlock": 1` is the important field.

On the RP2040, each GPIO can only reach one specific I²C block. **GPIO 26
and 27 belong to block 1 — they physically cannot be driven by block 0.**
In the Arduino-Pico core, `Wire` is block 0 and `Wire1` is block 1.

The firmware called:

```cpp
Wire.setSDA(26);   // asking BLOCK 0 to use pins only block 1 can reach
Wire.setSCL(27);
```

That fails silently. No ACK ever comes back, the presence probe reports
nothing on the bus, and the code correctly concludes "no display present"
and skips drawing — producing exactly the blank screen observed. The
one-word fix is `Wire1`.

This was never tested, because every attempt to run the I²C scanner
coincided with the USB instability described below.

## Did the pinout errors or the dead button cause it?

**No.** Worth stating clearly since both were real bugs found later:

- The **button pin errors** were on GPIO 0–14 and 20–22. The display is on
  26/27. Different pins, different peripheral. Reading a button wrong
  cannot stop an I²C device from ACKing.
- The **dead Turbo switch** is an open circuit on one input pin. It has no
  path to the I²C bus at all.
- The **`Wire` vs `Wire1` mistake** is, however, the *same class* of error
  as the pin bugs: a value was guessed from convention instead of read out
  of the backup file. `"i2cBlock": 1` was sitting in that JSON the whole
  time, exactly like the real pin numbers were.

The earlier **SH1106-instead-of-SSD1306** theory is still possible but is
now the second-best explanation. It should not be investigated until the
bus fix is tested, because a wrong-bus setup looks identical to a
wrong-chip setup from the outside: silence either way.

## Confounder: USB instability

Several scan attempts were lost to enumeration failures
(`device descriptor read/64, error -110`). This was **not** caused by the
display code — a minimal sketch containing no display or MIDI code failed
the same way. It was traced to USB hubs, marginal cables, and an xHCI
controller wedged by repeated BOOTSEL cycling. Plug directly into the
machine, and reboot if a port stops enumerating.

One genuine display-code bug *was* found and fixed during this: the
original `display.begin()` call could block long enough to stall USB
enumeration before the host finished its handshake, making the whole device
undetectable. The current code probes for an ACK with a bounded timeout
before calling `begin()`, so a missing panel can never hang boot.

## Is the web configurator still reachable?

**No.** `http://192.168.7.1` is a GP2040-CE feature — that firmware ships an
embedded HTTP server and presents itself as a USB network adapter. This
firmware is a MIDI device and serves nothing. The address returns only
while GP2040-CE is flashed.

It would not identify the panel anyway: the configurator exposes wiring
configuration (pins, address, speed), not the controller chip. It has no
"which silicon is this" readout, and the backup JSON contains no chip field.

**But reflashing GP2040-CE is still the single most useful diagnostic**, for
a different reason: it is a known-good driver for this exact panel. So

- **screen works under GP2040-CE** → panel and wiring are fine, the fault
  is entirely in this firmware, and `Wire1` is almost certainly it.
- **screen stays blank under GP2040-CE too** → the panel or its cable is
  dead, and no amount of firmware work will help.

That single test cleanly separates hardware from software, which nothing
tried so far has done.

## Next steps, in order

1. **Flash stock GP2040-CE and look at the screen.** Hardware or software,
   answered in one step. Restore afterwards with the saved `.uf2` and JSON.
2. If it works there, **run the two-bus scanner** (`projects/i2c_scan`,
   already written — scans `Wire1` on 26/27 *and* `Wire` on its defaults)
   and read the result over USB serial.
3. If something ACKs at `0x3C` on `Wire1`, re-enable the display code with
   `Wire1` throughout. The rendering code still exists in git history and
   can be restored rather than rewritten — it drew mode, scale/kit, chord
   modifiers, record prompts and saved-key indicators.
4. Only if `Wire1` ACKs but `SSD1306` init still fails should the SH1106
   theory be revisited (`Adafruit SH110X` library).
