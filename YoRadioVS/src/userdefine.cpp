#include <Wire.h>
#include "core/options.h"     // <-- ensure BUFLEN and other macros are defined
#include "core/config.h"
#include "core/player.h"
#include "MPR121Touch/src/MPR121Touch.h"
#include "userdefine.h"

namespace {
  constexpr uint8_t OPTO_INPUT_SELECTOR_PIN = 1;
  constexpr uint8_t OPTO_AUX1_PIN = 2;
  constexpr uint8_t OPTO_AUX2_PIN = 40;
  constexpr uint8_t OPTO_AUX3_PIN = 39;

  inline void pulseOpto(uint8_t pin, uint16_t durationMs) {
    digitalWrite(pin, HIGH);
    delay(durationMs);
    digitalWrite(pin, LOW);
  }
}

void optocouplers_setup() {
  pinMode(OPTO_INPUT_SELECTOR_PIN, OUTPUT);
  pinMode(OPTO_AUX1_PIN, OUTPUT);
  pinMode(OPTO_AUX2_PIN, OUTPUT);
  pinMode(OPTO_AUX3_PIN, OUTPUT);

  digitalWrite(OPTO_INPUT_SELECTOR_PIN, LOW);
  digitalWrite(OPTO_AUX1_PIN, LOW);
  digitalWrite(OPTO_AUX2_PIN, LOW);
  digitalWrite(OPTO_AUX3_PIN, LOW);
}

void opto_input_selector_pulse() {
  pulseOpto(OPTO_INPUT_SELECTOR_PIN, 20);
}

uint8_t opto_input_selector_cycle() {
  // 0=RADIO, 1=BT, 2=AUX
  static uint8_t inputState = 0;
  inputState = (inputState + 1) % 3;

  if (inputState == 0) {
    // AUX -> RADIO requires two pulses
    opto_input_selector_pulse();
    delay(20);
    opto_input_selector_pulse();
    return inputState;
  }

  // RADIO->BT or BT->AUX requires one pulse
  opto_input_selector_pulse();
  return inputState;
}

void opto_aux1_pulse(uint16_t durationMs) {
  pulseOpto(OPTO_AUX1_PIN, durationMs);
}

void opto_aux2_pulse(uint16_t durationMs) {
  pulseOpto(OPTO_AUX2_PIN, durationMs);
}

void opto_aux3_pulse(uint16_t durationMs) {
  pulseOpto(OPTO_AUX3_PIN, durationMs);
}

// forward-declare / implement yoRadio actions used by the MPR121 handler
// (these resolve the undefined-reference linker errors)

void yoradio_toggle_play() {
  player.toggle();
}

void yoradio_vol_up(int step) {
  uint8_t cur = config.store.volume;
  int nv = cur + step;
  if (nv > 254) nv = 254;
  player.setVol((uint8_t)nv);
}

void yoradio_vol_down(int step) {
  uint8_t cur = config.store.volume;
  int nv = (int)cur - step;
  if (nv < 0) nv = 0;
  player.setVol((uint8_t)nv);
}

void yoradio_next_station() {
  player.next();
}

void yoradio_prev_station() {
  player.prev();
}

// Simple mute toggle (remembers previous volume)
static int16_t _mpr121_prev_volume = -1;
void yoradio_toggle_mute() {
  if (_mpr121_prev_volume < 0) {
    _mpr121_prev_volume = config.store.volume;
    player.setVol(0);
  } else {
    player.setVol((uint8_t)_mpr121_prev_volume);
    _mpr121_prev_volume = -1;
  }
}

// Lightweight favorites storage (RAM-backed). Replace with persistent storage later.
static uint16_t _mpr121_favorites[12] = {0};

void yoradio_save_favorite(uint8_t btn) {
  if (btn >= sizeof(_mpr121_favorites)/sizeof(_mpr121_favorites[0])) return;
  _mpr121_favorites[btn] = config.lastStation();
  // TODO: persist to config/EEPROM if you want favorites retained across reboots
}

void yoradio_goto_favorite(uint8_t btn) {
  if (btn >= sizeof(_mpr121_favorites)/sizeof(_mpr121_favorites[0])) return;
  uint16_t st = _mpr121_favorites[btn];
  if (st == 0) return;
  player.sendCommand({PR_PLAY, (int)st});
}

// MPR121 instance (keep address macro or replace with literal if needed)
static MPR121Touch touch(MPR121_ADDR, &Wire);

static void handleTouch(uint8_t e, MPR121Event ev) {
  if (ev == MPR121Event::SHORT_CLICK) {
    switch (e) {
      case 0: yoradio_toggle_play(); break;
      case 1: yoradio_prev_station();     break;
      case 2: yoradio_next_station();   break;
      case 3: yoradio_goto_favorite(1);break;
      case 4: yoradio_goto_favorite(2);break;
	  case 5: yoradio_goto_favorite(3); break;
	  case 6: yoradio_goto_favorite(4); break;
	  case 7: yoradio_goto_favorite(5); break;
      case 8: yoradio_goto_favorite(6); break;                    
      case 9: yoradio_goto_favorite(7); break;
	  case 10: yoradio_goto_favorite(8); break;
	  case 11: yoradio_goto_favorite(9); break;


      default: break;
    }
  } else if (ev == MPR121Event::LONG_PRESS) {
    switch (e) {
      case 1:
      case 2: yoradio_toggle_mute(); break;
      // for favorites: treat long press as "save favorite"
      case 3: yoradio_save_favorite(1); break;
      case 4: yoradio_save_favorite(2); break;
      case 5: yoradio_save_favorite(3); break;
      case 6: yoradio_save_favorite(4); break;
      case 7: yoradio_save_favorite(5); break;
      case 8: yoradio_save_favorite(6); break;
      case 9: yoradio_save_favorite(7); break;
      case 10: yoradio_save_favorite(8); break;
      case 11: yoradio_save_favorite(9); break;
      default: break;
    }
  }
}

void mpr121_setup() {
  // more sensitive for long wires:
  touch.setThresholds(6, 3);      // touch, release (try 6/3, 8/4, ... )
  touch.setDebounceMs(40);        // reduce chatter, larger = fewer false triggers
  touch.setShortClickMs(350);
  touch.setLongPressMs(700);
  touch.setClockFrequency(100000); // optional: slower I2C clock, try 100k or 400k
  touch.onEvent(handleTouch);
  if (!touch.begin()) {
    Serial.println("[MPR121] Not found!");
  } else {
    Serial.println("[MPR121] OK");
  }
}

void mpr121_loop() {
  touch.update();
}