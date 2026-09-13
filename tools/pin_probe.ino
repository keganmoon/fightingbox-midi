/*
 * FightingBox pin prober.
 *
 * Watches every GPIO the GP2040-CE backup listed as a button and, on each
 * press, sends a MIDI Note On whose NOTE NUMBER IS THE GPIO NUMBER.
 * Press GPIO 4 -> note 4. No name mapping, no assumptions - whatever the
 * capture shows is literally the pin under that key.
 *
 * Flash, press each button one at a time, read the notes, done.
 */

#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// Every pin the stock GP2040-CE config used for a button.
const uint8_t PINS[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,20,21,22};
const uint8_t N = sizeof(PINS);

bool state[N]   = {false};
bool lastRaw[N] = {false};
unsigned long lastChange[N] = {0};

void setup() {
  for (uint8_t i = 0; i < N; i++) pinMode(PINS[i], INPUT_PULLUP);
  usb_midi.setStringDescriptor("FightingBox PROBE");
  MIDI.begin(MIDI_CHANNEL_OMNI);
  while (!TinyUSBDevice.mounted()) delay(1);
}

void loop() {
  MIDI.read();
  for (uint8_t i = 0; i < N; i++) {
    bool raw = (digitalRead(PINS[i]) == LOW);
    if (raw != lastRaw[i]) { lastChange[i] = millis(); lastRaw[i] = raw; }
    if (millis() - lastChange[i] > 8 && state[i] != raw) {
      state[i] = raw;
      // note number == GPIO number
      if (raw) MIDI.sendNoteOn(PINS[i], 100, 1);
      else     MIDI.sendNoteOff(PINS[i], 0, 1);
    }
  }
#ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
#endif
}
