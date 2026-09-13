/*
 * I2C probe that reports over MIDI (serial has been unreliable here).
 *
 * The GP2040-CE backup says the OLED is at 0x3C on SDA=26, SCL=27 with
 * "i2cBlock": 1. On the RP2040, GPIO26/27 can ONLY belong to I2C block 1,
 * which the Arduino-Pico core exposes as Wire1 - NOT Wire (block 0).
 * Earlier firmware called Wire.setSDA(26), which cannot work.
 *
 * This scans both buses and reports findings as MIDI notes:
 *
 *   channel 1, note = address   -> found on Wire1 (block 1, GPIO 26/27)
 *   channel 2, note = address   -> found on Wire  (block 0, GPIO 4/5)
 *   channel 16, note 1          -> heartbeat, proves the sketch is alive
 *
 * 0x3C = 60 decimal, so "channel 1, note 60" means the display answered
 * on the bus the config file specified all along.
 */

#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Wire.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

void scanBus(TwoWire &bus, uint8_t channel) {
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      MIDI.sendNoteOn(addr, 100, channel);
      delay(30);
      MIDI.sendNoteOff(addr, 0, channel);
      delay(30);
    }
  }
}

void setup() {
  // Block 1 on the pins the backup specifies - where the OLED should be.
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  Wire1.begin();

  // Block 0 on its default pins, for completeness.
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  usb_midi.setStringDescriptor("FightingBox I2C PROBE");
  MIDI.begin(MIDI_CHANNEL_OMNI);
  while (!TinyUSBDevice.mounted()) delay(1);
  delay(500);
}

void loop() {
  MIDI.read();

  MIDI.sendNoteOn(1, 100, 16);      // heartbeat
  delay(30);
  MIDI.sendNoteOff(1, 0, 16);
  delay(200);

  scanBus(Wire1, 1);
  scanBus(Wire,  2);

  delay(2000);

#ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
#endif
}
