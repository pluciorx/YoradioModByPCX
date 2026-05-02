#include <Wire.h>
#include <SPIFFS.h>
#include "core/options.h"     // <-- ensure BUFLEN and other macros are defined
#include "core/config.h"
#include "core/player.h"
#include "core/display.h"
#include "MPR121Touch/src/MPR121Touch.h"
#include "userdefine.h"

namespace {
  constexpr uint8_t OPTO_INPUT_SELECTOR_PIN = OPTO1_PIN;
  constexpr uint8_t OPTO_AUX1_PIN = OPTO2_PIN;
  constexpr uint8_t OPTO_AUX2_PIN = OPTO3_PIN;
  constexpr uint8_t OPTO_AUX3_PIN = OPTO4_PIN;
  uint8_t g_inputState = 0;
  volatile bool g_inputPending = false; // set by encoder, consumed by display loop

 
  ///
  inline void pulseOpto(uint8_t pin, uint16_t durationMs) {
    digitalWrite(pin, HIGH);
    delay(durationMs);
    digitalWrite(pin, LOW);
  }
}

// Called from the display task to flush pending input-state changes.
// Returns true when a new runtime input state was applied.
bool opto_input_selector_flush(uint8_t *appliedState) {
  if (!g_inputPending) return false;
  g_inputPending = false;
  uint8_t inputState = g_inputState;
  if (appliedState) *appliedState = inputState;

  if (inputState == 0) {
    config.loadStation(config.lastStation());
    player.sendCommand({ PR_PLAY, config.lastStation() });
  } else if (inputState == 1) {
    player.sendCommand({ PR_STOP, 0 });
    config.setStation("INPUT: BLUETOOTH");
    config.setTitle("");
  } else {
    player.sendCommand({ PR_STOP, 0 });
    config.setStation("INPUT: AUX");
    config.setTitle("");
  }

  return true;
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

  g_inputState = 0;
  g_inputPending = false;
}

void opto_input_selector_pulse() {
  pulseOpto(OPTO_INPUT_SELECTOR_PIN, 20);
}

uint8_t opto_input_selector_current() {
  return g_inputState;
}

uint8_t opto_input_selector_set(uint8_t targetState) {
  targetState %= 3;
  if (targetState != g_inputState) {
    // Forward-only pulse model: RADIO->BT=1, BT->AUX=1, AUX->RADIO=2
    // Walk forward one step at a time until target is reached.
    while (g_inputState != targetState) {
      uint8_t stepPulses = (g_inputState == 2) ? 2 : 1;
      for (uint8_t i = 0; i < stepPulses; ++i) {
        opto_input_selector_pulse();
        if (i + 1 < stepPulses) delay(50); // longer gap between 2-pulse AUX->RADIO sequence
      }
      g_inputState = (uint8_t)((g_inputState + 1) % 3);
      if (g_inputState != targetState) delay(50); // let bus settle before next step
    }
  }
  // Always mark pending so the display task will catch the latest state
  // even if multiple steps arrived before the previous update was rendered.
  g_inputPending = true;
  return g_inputState;
}

uint8_t opto_input_selector_step(bool toRight) {
  if (!toRight) return g_inputState;
  uint8_t targetState = (uint8_t)((g_inputState + 1) % 3);
  return opto_input_selector_set(targetState);
}

uint8_t opto_input_selector_cycle() {
  return opto_input_selector_step(true);
}

void toggleOpto(uint8_t pin, bool state) {
    digitalWrite(pin, state ? HIGH : LOW);
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
static bool _mpr121_favoritesLoaded = false;
static constexpr const char* MPR121_FAVORITES_PATH = "/mpr121_favorites.bin";

static void mpr121_load_favorites() {
  if (_mpr121_favoritesLoaded) return;

  memset(_mpr121_favorites, 0, sizeof(_mpr121_favorites));
  if (!SPIFFS.exists(MPR121_FAVORITES_PATH)) {
    _mpr121_favoritesLoaded = true;
    return;
  }

  File f = SPIFFS.open(MPR121_FAVORITES_PATH, "r");
  if (!f) {
    Serial.println("[MPR121] favorites open(read) failed");
    _mpr121_favoritesLoaded = true;
    return;
  }

  size_t read = f.read((uint8_t*)_mpr121_favorites, sizeof(_mpr121_favorites));
  f.close();

  if (read < sizeof(_mpr121_favorites)) {
    memset(((uint8_t*)_mpr121_favorites) + read, 0, sizeof(_mpr121_favorites) - read);
  }

  _mpr121_favoritesLoaded = true;
}

static void mpr121_save_favorites() {
  File f = SPIFFS.open(MPR121_FAVORITES_PATH, "w");
  if (!f) {
    Serial.println("[MPR121] favorites open(write) failed");
    return;
  }
  f.write((const uint8_t*)_mpr121_favorites, sizeof(_mpr121_favorites));
  f.close();
}

void yoradio_save_favorite(uint8_t btn) {
  if (btn >= sizeof(_mpr121_favorites)/sizeof(_mpr121_favorites[0])) return;
  mpr121_load_favorites();
  _mpr121_favorites[btn] = config.lastStation();
  mpr121_save_favorites();
  config.showFavoriteSavedMarker();
}

void yoradio_goto_favorite(uint8_t btn) {
  if (btn >= sizeof(_mpr121_favorites)/sizeof(_mpr121_favorites[0])) return;
  mpr121_load_favorites();
  uint16_t st = _mpr121_favorites[btn];
  if (st == 0) return;
  player.sendCommand({PR_PLAY, (int)st});
}


#if MPR121
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
  mpr121_load_favorites();
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
#endif // MPR121