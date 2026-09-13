/*
 * I2C scanner for the FightingBox OLED.
 *
 * The GP2040-CE backup says the display is on SDA=26, SCL=27 with
 * "i2cBlock": 1. On the RP2040, GPIO26/27 can ONLY belong to I2C block 1,
 * which the Arduino-Pico core exposes as Wire1 (Wire is block 0). Earlier
 * firmware called Wire.setSDA(26), asking block 0 to drive pins it cannot
 * reach - that fails silently and looks exactly like "no display".
 *
 * This sketch scans BOTH blocks and reports over USB serial so we can see
 * which bus and address actually respond.
 */

#include <Wire.h>

void scan(TwoWire &bus, const char *label) {
  Serial.print("--- ");
  Serial.print(label);
  Serial.println(" ---");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      Serial.print("  device at 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println("  (nothing responded)");
}

void setup() {
  Serial.begin(115200);

  // Block 1 on the pins the backup specifies - the expected home of the OLED.
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  Wire1.begin();

  // Block 0 on its default pins, just to see everything on the board.
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  delay(1500);
}

void loop() {
  Serial.println();
  Serial.println("=== I2C scan ===");
  scan(Wire1, "Wire1 / block 1 / GPIO 26+27  <- display should be here");
  scan(Wire,  "Wire  / block 0 / GPIO 4+5");
  delay(2500);
}
