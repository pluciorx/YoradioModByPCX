#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Encoder-2 on-screen menu
//
// Usage:
//   enc2menu_enter()   – open menu (called on enc2 long-press from PLAYER)
//   enc2menu_exit()    – close menu and return to PLAYER
//   enc2menu_rotate()  – call with +1 / -1 from encoder2 delta when active
//   enc2menu_select()  – call on enc2 button click when active
// ---------------------------------------------------------------------------

// ---- Menu item identifiers (add more here to extend the menu) ----
enum Enc2MenuItem : uint8_t {
	MENU_SS_ANIM_STOP = 0,  // Screensaver animation while stopped
  MENU_SS_ANIM_PLAY,      // Screensaver animation while playing
  MENU_SS_TIMEOUT,        // Screensaver timeout (seconds), when stopped
  MENU_SS_PLAYING_TIMEOUT,// Screensaver timeout (seconds), while playing
  MENU_AUX1_TOGGLE,       // Optocoupler AUX1 latched ON/OFF
  MENU_AUX2_TOGGLE,       // Optocoupler AUX2 latched ON/OFF
  MENU_AUX3_TOGGLE,       // Optocoupler AUX3 latched ON/OFF
  MENU_ITEM_COUNT         // keep last
};

// ---- Public display buffers written by enc2menu logic, read by display ----
extern char enc2menu_label[48];   // current item label, e.g. "INPUT"
extern char enc2menu_value[32];   // current item value, e.g. "RADIO"

// ---- API ----
void enc2menu_enter();
void enc2menu_exit();
void enc2menu_rotate(int8_t delta);  // +1 = next item, -1 = prev item
void enc2menu_select();             // activate / toggle / cycle current item
