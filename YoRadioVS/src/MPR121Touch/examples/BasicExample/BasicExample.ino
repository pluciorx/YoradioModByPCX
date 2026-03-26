/**
 * MPR121Touch – BasicExample
 *
 * Wiring (ESP32 default I2C):
 *   SDA → GPIO 21
 *   SCL → GPIO 22
 *   VCC → 3.3 V
 *   GND → GND
 *   ADDR pin floating → I2C address 0x5A
 *
 * Open Serial Monitor at 115200 baud.
 */

#include <Wire.h>
#include "MPR121Touch.h"

// ── Create instance ───────────────────────────────────────────────────────────
MPR121Touch touch(MPR121_ADDR, &Wire);

// ── Event callback ────────────────────────────────────────────────────────────
void onTouch(uint8_t electrode, MPR121Event event) {
  switch (event) {
    case MPR121Event::TOUCH:
      Serial.printf("[E%02d] Touched\n", electrode);
      break;
    case MPR121Event::RELEASE:
      Serial.printf("[E%02d] Released\n", electrode);
      break;
    case MPR121Event::SHORT_CLICK:
      Serial.printf("[E%02d] SHORT CLICK\n", electrode);
      break;
    case MPR121Event::LONG_PRESS:
      Serial.printf("[E%02d] LONG PRESS\n", electrode);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // ── Optional: customise timings before begin() ───────────────────────────
  touch.setDebounceMs(20);      // debounce window
  touch.setShortClickMs(400);   // max held-time to count as a short click
  touch.setLongPressMs(800);    // held-time to fire long-press

  // ── Register callback ────────────────────────────────────────────────────
  touch.onEvent(onTouch);

  // ── Initialise ───────────────────────────────────────────────────────────
  if (!touch.begin()) {
    Serial.println("MPR121 not found – check wiring / address!");
    while (true) delay(1000);
  }
  Serial.println("MPR121 ready.");
}

void loop() {
  touch.update();   // must be called regularly; handles timing internally
  delay(10);        // ~100 Hz poll rate is plenty
}
