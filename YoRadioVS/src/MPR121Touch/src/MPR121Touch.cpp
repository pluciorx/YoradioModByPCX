#include "MPR121Touch.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
MPR121Touch::MPR121Touch(uint8_t addr, TwoWire* wire)
  : _wire(wire), _addr(addr) {}

// ─────────────────────────────────────────────────────────────────────────────
// begin()
// ─────────────────────────────────────────────────────────────────────────────
bool MPR121Touch::begin() {
  _wire->begin();

  if (!isConnected()) return false;

  softReset();
  delay(1);          // datasheet: wait ≥1 ms after reset

  _configure();
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal chip configuration
// ─────────────────────────────────────────────────────────────────────────────
void MPR121Touch::_configure() {
  // Must be in stop-mode (ELECTRODE_CONFIG = 0x00) to write most registers.
  writeReg(MPR121Reg::ELECTRODE_CONFIG, 0x00);

  // ── Baseline filtering (NXP application-note values) ─────────────────────
  writeReg(MPR121Reg::MHD_RISING,  0x01);
  writeReg(MPR121Reg::NHD_RISING,  0x01);
  writeReg(MPR121Reg::NCL_RISING,  0x00);
  writeReg(MPR121Reg::FDL_RISING,  0x00);
  writeReg(MPR121Reg::MHD_FALLING, 0x01);
  writeReg(MPR121Reg::NHD_FALLING, 0x01);
  writeReg(MPR121Reg::NCL_FALLING, 0xFF);
  writeReg(MPR121Reg::FDL_FALLING, 0x02);

  // ── Touch / release thresholds for all 12 electrodes ─────────────────────
  setThresholds(_touchThreshold, _releaseThreshold);

  // ── Debounce: 1 sample for both touch and release ─────────────────────────
  // Bits[2:0] = release samples, bits[6:4] = touch samples (0 = 1 sample)
  writeReg(MPR121Reg::DEBOUNCE, 0x00);

  // ── AFE / Filter configuration ────────────────────────────────────────────
  // AFE1: CDC=16µA, CDT=0.5µs  →  0x10
  // AFE2: SFI=4 samples, ESI=16ms  →  0x20
  writeReg(MPR121Reg::AFE_CONFIG1, 0x10);
  writeReg(MPR121Reg::AFE_CONFIG2, 0x20);

  // ── Auto-configuration (USL/LSL/TL calculation for 3.3 V supply) ──────────
  writeReg(MPR121Reg::AUTO_CONFIG0, 0x8F);
  writeReg(MPR121Reg::AUTO_CONFIG1, 0x00);
  writeReg(MPR121Reg::UPLIMIT,      200);   // USL  = VDD·0.9/VDD·256  → ~200 @ 3.3 V
  writeReg(MPR121Reg::LOWLIMIT,     130);   // LSL  = USL·0.65
  writeReg(MPR121Reg::TARGETLIMIT,  180);   // TL   = USL·0.9

  // ── Start all 12 electrodes (CL=10b, ELEPROX=00, ELE=1100) ───────────────
  writeReg(MPR121Reg::ELECTRODE_CONFIG, 0x8C);
}

// ─────────────────────────────────────────────────────────────────────────────
// setThresholds()  – applies same threshold to all 12 electrodes
// ─────────────────────────────────────────────────────────────────────────────
void MPR121Touch::setThresholds(uint8_t touch, uint8_t release) {
  _touchThreshold   = touch;
  _releaseThreshold = release;
  for (uint8_t i = 0; i < MPR121_NUM_ELECTRODES; i++) {
    setElectrodeThresholds(i, touch, release);
  }
}

void MPR121Touch::setElectrodeThresholds(uint8_t e,
                                         uint8_t touch,
                                         uint8_t release) {
  if (e >= MPR121_NUM_ELECTRODES) return;
  writeReg(MPR121Reg::TOUCH_THRESHOLD   + (e * 2), touch);
  writeReg(MPR121Reg::RELEASE_THRESHOLD + (e * 2), release);
}

// ─────────────────────────────────────────────────────────────────────────────
// setClockFrequency()
// ─────────────────────────────────────────────────────────────────────────────
void MPR121Touch::setClockFrequency(uint32_t hz) {
  _wire->setClock(hz);
}

// ─────────────────────────────────────────────────────────────────────────────
// update() – call from loop()
// ─────────────────────────────────────────────────────────────────────────────
uint16_t MPR121Touch::update() {
  uint16_t raw = readReg16(MPR121Reg::TOUCH_STATUS_LSB) & 0x0FFF;
  uint32_t now = millis();

  for (uint8_t e = 0; e < MPR121_NUM_ELECTRODES; e++) {
    bool rawNow = (raw >> e) & 0x01;
    _processElectrode(e, rawNow, now);
  }
  return _touchedMask;
}

// ─────────────────────────────────────────────────────────────────────────────
// _processElectrode()  – debounce + event state machine
// ─────────────────────────────────────────────────────────────────────────────
void MPR121Touch::_processElectrode(uint8_t e, bool rawNow, uint32_t now) {
  ElectrodeState& s = _state[e];

  // ── Debounce ──────────────────────────────────────────────────────────────
  if (rawNow != s.rawTouched) {
    s.rawTouched   = rawNow;
    s.lastChangeMs = now;
  }

  bool debounced = s.touched;
  if ((now - s.lastChangeMs) >= _debounceMs) {
    debounced = s.rawTouched;
  }

  // ── Rising edge (touch) ───────────────────────────────────────────────────
  if (debounced && !s.touched) {
    s.touched   = true;
    s.longFired = false;
    s.touchTime = now;
    _touchedMask |= (1u << e);
    _emit(e, MPR121Event::TOUCH);
  }

  // ── Falling edge (release) ────────────────────────────────────────────────
  if (!debounced && s.touched) {
    s.touched    = false;
    _touchedMask &= ~(1u << e);
    _emit(e, MPR121Event::RELEASE);

    uint32_t held = now - s.touchTime;
    if (!s.longFired && held < _shortClickMs) {
      _emit(e, MPR121Event::SHORT_CLICK);
    }
  }

  // ── Long-press detection (fires once while held) ──────────────────────────
  if (s.touched && !s.longFired) {
    if ((now - s.touchTime) >= _longPressMs) {
      s.longFired = true;
      _emit(e, MPR121Event::LONG_PRESS);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// _emit()
// ─────────────────────────────────────────────────────────────────────────────
void MPR121Touch::_emit(uint8_t e, MPR121Event ev) {
  if (_callback) _callback(e, ev);
}

// ─────────────────────────────────────────────────────────────────────────────
// isTouched()
// ─────────────────────────────────────────────────────────────────────────────
bool MPR121Touch::isTouched(uint8_t electrode) const {
  if (electrode >= MPR121_NUM_ELECTRODES) return false;
  return (_touchedMask >> electrode) & 0x01;
}

// ─────────────────────────────────────────────────────────────────────────────
// softReset()
// ─────────────────────────────────────────────────────────────────────────────
bool MPR121Touch::softReset() {
  return writeReg(MPR121Reg::SOFT_RESET, 0x63);
}

// ─────────────────────────────────────────────────────────────────────────────
// isConnected()
// ─────────────────────────────────────────────────────────────────────────────
bool MPR121Touch::isConnected() {
  _wire->beginTransmission(_addr);
  return (_wire->endTransmission() == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// writeReg() / readReg() / readReg16()
// ─────────────────────────────────────────────────────────────────────────────
bool MPR121Touch::writeReg(uint8_t reg, uint8_t val) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  _wire->write(val);
  return (_wire->endTransmission() == 0);
}

uint8_t MPR121Touch::readReg(uint8_t reg) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  _wire->endTransmission(false);           // repeated start
  _wire->requestFrom(_addr, (uint8_t)1);
  return _wire->available() ? _wire->read() : 0xFF;
}

uint16_t MPR121Touch::readReg16(uint8_t reg) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  _wire->endTransmission(false);
  _wire->requestFrom(_addr, (uint8_t)2);
  uint16_t lo = _wire->available() ? _wire->read() : 0;
  uint16_t hi = _wire->available() ? _wire->read() : 0;
  return (hi << 8) | lo;
}
