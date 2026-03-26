/**
 * MPR121Touch – yoRadio integration example
 *
 * Drop this pattern into your yoRadio user-defined button handler.
 *
 * In yoRadio the custom button/encoder handler lives in
 *   src/userdefine.h  +  src/userdefine.cpp
 * (or equivalent user-patch files depending on your fork version).
 *
 * ── Suggested electrode mapping (edit to taste) ─────────────────────────────
 *
 *   E0  → PLAY / PAUSE
 *   E1  → VOLUME UP        (short) / MUTE (long)
 *   E2  → VOLUME DOWN      (short) / MUTE (long)
 *   E3  → NEXT STATION
 *   E4  → PREVIOUS STATION
 *   E5  → STATION +10
 *   E6  → STATION -10
 *   E7  → SLEEP TIMER toggle
 *   E8  → DISPLAY TOGGLE
 *   E9  → (free)
 *   E10 → (free)
 *   E11 → WIFI INFO / reboot on long press
 */

#include <Wire.h>
#include "MPR121Touch.h"

// ── Forward-declare yoRadio actions you need ──────────────────────────────────
// (These are already declared inside yoRadio – just call them)
extern void yoradio_toggle_play();
extern void yoradio_vol_up(int step);
extern void yoradio_vol_down(int step);
extern void yoradio_next_station();
extern void yoradio_prev_station();
extern void yoradio_toggle_mute();

// ── MPR121 instance ───────────────────────────────────────────────────────────
static MPR121Touch touch(MPR121_ADDR, &Wire);

// ── Event handler ─────────────────────────────────────────────────────────────
static void handleTouch(uint8_t e, MPR121Event ev) {
  if (ev == MPR121Event::SHORT_CLICK) {
    switch (e) {
      case 0: yoradio_toggle_play();    break;
      case 1: yoradio_vol_up(5);        break;
      case 2: yoradio_vol_down(5);      break;
      case 3: yoradio_next_station();   break;
      case 4: yoradio_prev_station();   break;
      default: break;
    }
  }

  if (ev == MPR121Event::LONG_PRESS) {
    switch (e) {
      case 1: // fall-through
      case 2: yoradio_toggle_mute(); break;
      default: break;
    }
  }
}

// ── Call from your setup() ────────────────────────────────────────────────────
void mpr121_setup() {
  // Adjust timings to taste
  touch.setDebounceMs(25);
  touch.setShortClickMs(350);
  touch.setLongPressMs(700);

  touch.onEvent(handleTouch);

  if (!touch.begin()) {
    Serial.println("[MPR121] Not found!");
  } else {
    Serial.println("[MPR121] OK");
  }
}

// ── Call from your loop() ─────────────────────────────────────────────────────
void mpr121_loop() {
  touch.update();
}
