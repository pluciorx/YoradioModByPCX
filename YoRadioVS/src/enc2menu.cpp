// ---------------------------------------------------------------------------
// enc2menu.cpp  –  Encoder-2 on-screen hardware menu
//
// Rotation  : scroll through items
// Click     : activate / cycle current item
// Long-press: open (from PLAYER) or close (from ENC2MENU) → handled in controls.cpp
// ---------------------------------------------------------------------------
#include "enc2menu.h"
#include "core/options.h"
#include "core/common.h"
#include "core/config.h"
#include "core/display.h"
#include "core/player.h"
#include "userdefine.h"
#include "displays/animations.h"   // AnimationType, ANIM_TYPE_COUNT

// ---- Display buffers (read by display.cpp) --------------------------------
char enc2menu_label[48] = {};
char enc2menu_value[32] = {};

// ---- Internal state -------------------------------------------------------
static uint8_t  _menuIdx   = 0;       // current item index

// AUX opto latched states (non-persistent, reset on boot)
static bool _aux1State = false;
static bool _aux2State = false;
static bool _aux3State = false;

static uint8_t _inputState = 0;

// ---- Helper: animation name for AnimationType index ----------------------
static const char* _animName(uint8_t idx) {
  switch ((AnimationType)idx) {
	case ANIM_FISH:        return "FISH";
	case ANIM_STARS:       return "STARS";
	case ANIM_WAVES:       return "WAVES";
	case ANIM_BALL:        return "BALL";
	case ANIM_SNAKE:       return "SNAKE";
	case ANIM_CLOCK_ONLY:  return "CLOCK";
	case ANIM_SOUND_METER: return "METER";
	default:               return "?";
  }
}

// ---- Helper: refresh label + value for current item, then push to display -
static void _refreshDisplay() {
  switch ((Enc2MenuItem)_menuIdx) {
	case MENU_AUX1_TOGGLE:
	  strlcpy(enc2menu_label, "AUX-1 (OPTO)", sizeof(enc2menu_label));
	  strlcpy(enc2menu_value, _aux1State ? "ON" : "OFF", sizeof(enc2menu_value));
	  break;
	case MENU_AUX2_TOGGLE:
	  strlcpy(enc2menu_label, "AUX-2 (OPTO)", sizeof(enc2menu_label));
	  strlcpy(enc2menu_value, _aux2State ? "ON" : "OFF", sizeof(enc2menu_value));
	  break;
	case MENU_AUX3_TOGGLE:
	  strlcpy(enc2menu_label, "AUX-3 (OPTO)", sizeof(enc2menu_label));
	  strlcpy(enc2menu_value, _aux3State ? "ON" : "OFF", sizeof(enc2menu_value));
	  break;

	case MENU_SS_ANIM_STOP: {
	  strlcpy(enc2menu_label, "SS ANIM STOP", sizeof(enc2menu_label));
	  strlcpy(enc2menu_value, _animName(config.store.lcdAnimationTypeStopped), sizeof(enc2menu_value));
	  break;
	}
	case MENU_SS_ANIM_PLAY: {
	  strlcpy(enc2menu_label, "SS ANIM PLAY", sizeof(enc2menu_label));
	  strlcpy(enc2menu_value, _animName(config.store.lcdAnimationTypePlaying), sizeof(enc2menu_value));
	  break;
	}
	case MENU_SS_TIMEOUT: {
	  strlcpy(enc2menu_label, "SS TIMEOUT s", sizeof(enc2menu_label));
	  snprintf(enc2menu_value, sizeof(enc2menu_value), "%u", (unsigned)config.store.screensaverTimeout);
	  break;
	}
	case MENU_SS_PLAYING_TIMEOUT: {
	  strlcpy(enc2menu_label, "SS PLY TMOUT s", sizeof(enc2menu_label));
	  snprintf(enc2menu_value, sizeof(enc2menu_value), "%u", (unsigned)config.store.screensaverPlayingTimeout);
	  break;
	}

	default:
	  strlcpy(enc2menu_label, "?", sizeof(enc2menu_label));
	  strlcpy(enc2menu_value, "", sizeof(enc2menu_value));
	  break;
  }

  display.putRequest(MENU_UPDATE);
}

// ---- Public API -----------------------------------------------------------

void enc2menu_enter() {
  _menuIdx = 0;
  _refreshDisplay();
  display.putRequest(NEWMODE, ENC2MENU);
}

void enc2menu_exit() {
  display.putRequest(NEWMODE, PLAYER);
}

void enc2menu_rotate(int8_t delta) {
  if (delta > 0) {
	_menuIdx = (_menuIdx + 1) % (uint8_t)MENU_ITEM_COUNT;
  } else {
	_menuIdx = (_menuIdx == 0) ? (uint8_t)(MENU_ITEM_COUNT - 1) : (_menuIdx - 1);
  }
  _refreshDisplay();
}

void enc2menu_select() {
  switch ((Enc2MenuItem)_menuIdx) {
	case MENU_AUX1_TOGGLE:
	  _aux1State = !_aux1State;
	  // 80 ms pulse turns the AUX1 relay on or off (toggle-type relay).
	  toggleOpto(OPTO2_PIN, _aux1State); // direct on/off control for AUX1 (pin 2)  
	  _refreshDisplay();
	  break;

	case MENU_AUX2_TOGGLE:
	  _aux2State = !_aux2State;
	  toggleOpto(OPTO3_PIN, _aux2State); // direct on/off control for AUX2 (pin 3)	 
	  _refreshDisplay();
	  break;

	case MENU_AUX3_TOGGLE:
	  _aux3State = !_aux3State;
	  toggleOpto(OPTO4_PIN, _aux3State); // direct on/off control for AUX3 (pin 4)	 
	  _refreshDisplay();
	  break;

	case MENU_SS_ANIM_STOP: {
	  uint8_t next = (config.store.lcdAnimationTypeStopped + 1) % (uint8_t)ANIM_TYPE_COUNT;
	  config.setLcdAnimationTypeStopped(next);
	  _refreshDisplay();
	  break;
	}
	case MENU_SS_ANIM_PLAY: {
	  uint8_t next = (config.store.lcdAnimationTypePlaying + 1) % (uint8_t)ANIM_TYPE_COUNT;
	  config.setLcdAnimationTypePlaying(next);
	  _refreshDisplay();
	  break;
	}
	case MENU_SS_TIMEOUT: {
	  // Step through: 10, 20, 30, 60, 120, 300 seconds
	  static const uint16_t timeoutSteps[] = { 10, 20, 30, 60, 120, 300 };
	  constexpr uint8_t stepCount = sizeof(timeoutSteps) / sizeof(timeoutSteps[0]);
	  uint8_t idx = 0;
	  for (uint8_t i = 0; i < stepCount - 1; i++) {
		if (config.store.screensaverTimeout <= timeoutSteps[i]) { idx = (i + 1) % stepCount; break; }
		idx = 0; // wrap to first if already at max
	  }
	  config.setScreensaverTimeout(timeoutSteps[idx]);
	  _refreshDisplay();
	  break;
	}
	case MENU_SS_PLAYING_TIMEOUT: {
	  static const uint16_t timeoutSteps[] = { 10, 20, 30, 60, 120, 300 };
	  constexpr uint8_t stepCount = sizeof(timeoutSteps) / sizeof(timeoutSteps[0]);
	  uint8_t idx = 0;
	  for (uint8_t i = 0; i < stepCount - 1; i++) {
		if (config.store.screensaverPlayingTimeout <= timeoutSteps[i]) { idx = (i + 1) % stepCount; break; }
		idx = 0;
	  }
	  config.setScreensaverPlayingTimeout(timeoutSteps[idx]);
	  _refreshDisplay();
	  break;
	}

	default:
	  break;
  }
}
