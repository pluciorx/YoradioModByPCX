#pragma once

#include <Arduino.h>
#include <Wire.h>

// ─── Default I2C address ────────────────────────────────────────────────────
#ifndef MPR121_ADDR
  #define MPR121_ADDR 0x5A
#endif

// ─── Number of electrodes ───────────────────────────────────────────────────
#define MPR121_NUM_ELECTRODES 12

// ─── Default timing (ms) ────────────────────────────────────────────────────
#define MPR121_DEFAULT_DEBOUNCE_MS    20
#define MPR121_DEFAULT_SHORT_CLICK_MS 400   // max duration for a short click
#define MPR121_DEFAULT_LONG_PRESS_MS  800   // min duration to trigger long press

// ─── MPR121 register map (subset used here) ─────────────────────────────────
namespace MPR121Reg {
  static constexpr uint8_t TOUCH_STATUS_LSB  = 0x00;
  static constexpr uint8_t TOUCH_STATUS_MSB  = 0x01;
  static constexpr uint8_t MHD_RISING        = 0x2B;
  static constexpr uint8_t NHD_RISING        = 0x2C;
  static constexpr uint8_t NCL_RISING        = 0x2D;
  static constexpr uint8_t FDL_RISING        = 0x2E;
  static constexpr uint8_t MHD_FALLING       = 0x2F;
  static constexpr uint8_t NHD_FALLING       = 0x30;
  static constexpr uint8_t NCL_FALLING       = 0x31;
  static constexpr uint8_t FDL_FALLING       = 0x32;
  static constexpr uint8_t TOUCH_THRESHOLD   = 0x41; // E0TTH (base, +2 per electrode)
  static constexpr uint8_t RELEASE_THRESHOLD = 0x42; // E0RTH (base, +2 per electrode)
  static constexpr uint8_t DEBOUNCE          = 0x5B;
  static constexpr uint8_t AFE_CONFIG1       = 0x5C;
  static constexpr uint8_t AFE_CONFIG2       = 0x5D;
  static constexpr uint8_t ELECTRODE_CONFIG  = 0x5E;
  static constexpr uint8_t AUTO_CONFIG0      = 0x7B;
  static constexpr uint8_t AUTO_CONFIG1      = 0x7C;
  static constexpr uint8_t UPLIMIT           = 0x7D;
  static constexpr uint8_t LOWLIMIT          = 0x7E;
  static constexpr uint8_t TARGETLIMIT       = 0x7F;
  static constexpr uint8_t SOFT_RESET        = 0x80;
}

// ─── Event types ─────────────────────────────────────────────────────────────
enum class MPR121Event : uint8_t {
  TOUCH       = 0,   // finger just touched
  RELEASE     = 1,   // finger just released (before click classification)
  SHORT_CLICK = 2,   // tap / short press
  LONG_PRESS  = 3,   // long press (fires once, while still held)
};

// ─── Callback signature ──────────────────────────────────────────────────────
// void myCallback(uint8_t electrode, MPR121Event event);
using MPR121Callback = void (*)(uint8_t electrode, MPR121Event event);

// ─── Per-electrode runtime state ─────────────────────────────────────────────
struct ElectrodeState {
  bool     touched       = false;
  bool     longFired     = false;  // long-press already sent for this press
  uint32_t touchTime     = 0;      // millis() when touch started
  uint32_t lastChangeMs  = 0;      // for debounce
  bool     rawTouched    = false;  // raw reading (pre-debounce)
};

// ─── Main class ──────────────────────────────────────────────────────────────
class MPR121Touch {
public:
  // Constructor
  // addr      – I2C address (0x5A–0x5D depending on ADDR pin)
  // wire      – TwoWire instance (&Wire or &Wire1 etc.)
  explicit MPR121Touch(uint8_t addr = MPR121_ADDR, TwoWire* wire = &Wire);

  // ── Initialisation ──────────────────────────────────────────────────────
  // Returns false if the chip is not found on the bus.
  bool begin();

  // Optional: change I2C clock after begin()
  void setClockFrequency(uint32_t hz);

  // ── Sensitivity (call before begin() or after soft-reset) ───────────────
  // touch threshold   : lower = more sensitive  (default 12)
  // release threshold : must be < touchThreshold (default 6)
  void setThresholds(uint8_t touch = 12, uint8_t release = 6);
  void setElectrodeThresholds(uint8_t electrode,
                              uint8_t touch, uint8_t release);

  // ── Timing configuration ────────────────────────────────────────────────
  void setDebounceMs(uint16_t ms)    { _debounceMs    = ms; }
  void setShortClickMs(uint16_t ms)  { _shortClickMs  = ms; }
  void setLongPressMs(uint16_t ms)   { _longPressMs   = ms; }

  uint16_t getDebounceMs()   const { return _debounceMs;   }
  uint16_t getShortClickMs() const { return _shortClickMs; }
  uint16_t getLongPressMs()  const { return _longPressMs;  }

  // ── Callback registration ───────────────────────────────────────────────
  void onEvent(MPR121Callback cb) { _callback = cb; }

  // ── Main poll function – call from loop() ───────────────────────────────
  // Returns bitmask of currently-touched electrodes (bits 0-11).
  uint16_t update();

  // ── Direct state queries ────────────────────────────────────────────────
  bool     isTouched(uint8_t electrode) const;
  uint16_t getTouchedMask() const { return _touchedMask; }

  // ── Hardware helpers ────────────────────────────────────────────────────
  bool     softReset();
  bool     isConnected();

  // ── Low-level register access ───────────────────────────────────────────
  bool    writeReg(uint8_t reg, uint8_t val);
  uint8_t readReg(uint8_t reg);
  uint16_t readReg16(uint8_t reg);

private:
  TwoWire*      _wire;
  uint8_t       _addr;

  uint16_t      _touchedMask   = 0;
  uint16_t      _debounceMs    = MPR121_DEFAULT_DEBOUNCE_MS;
  uint16_t      _shortClickMs  = MPR121_DEFAULT_SHORT_CLICK_MS;
  uint16_t      _longPressMs   = MPR121_DEFAULT_LONG_PRESS_MS;

  uint8_t       _touchThreshold   = 12;
  uint8_t       _releaseThreshold = 6;

  MPR121Callback _callback = nullptr;
  ElectrodeState _state[MPR121_NUM_ELECTRODES];

  void _configure();
  void _processElectrode(uint8_t e, bool rawNow, uint32_t now);
  void _emit(uint8_t e, MPR121Event ev);
};
