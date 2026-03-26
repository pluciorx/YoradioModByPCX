# MPR121Touch

Lightweight Arduino/ESP32 library for the NXP **MPR121** 12-channel capacitive touch controller.  
Designed for use with the **[yoRadio](https://github.com/e2002/yoradio)** project but fully standalone.

---

## Features

| Feature | Details |
|---|---|
| Events | `TOUCH`, `RELEASE`, `SHORT_CLICK`, `LONG_PRESS` |
| Timing | Debounce, short-click max duration, long-press threshold – all configurable |
| Long-press | Fires **once** while the finger is still held |
| Short-click | Fires on **release** only if held time < `shortClickMs` and no long-press fired |
| Callback | Single `void cb(uint8_t electrode, MPR121Event event)` function |
| I2C | Works with any `TwoWire` instance (`Wire`, `Wire1`, …) |
| Addresses | 0x5A (default) / 0x5B / 0x5C / 0x5D via ADDR pin |

---

## Wiring (ESP32)

```
MPR121   ESP32
──────   ─────────────────────
VCC   →  3.3 V
GND   →  GND
SDA   →  GPIO 21  (or custom)
SCL   →  GPIO 22  (or custom)
ADDR  →  GND / 3.3V / SDA / SCL  → sets I2C address
IRQ   →  (not used – library polls the status registers)
```

> **Note:** The library uses polling, not the IRQ pin. This keeps the code  
> simple and avoids interrupt-safe considerations in yoRadio's main loop.

---

## Installation

1. Download or clone this folder into your Arduino `libraries/` directory as `MPR121Touch`.
2. Restart the Arduino IDE (or PlatformIO will pick it up automatically).

---

## Quick Start

```cpp
#include <Wire.h>
#include "MPR121Touch.h"

MPR121Touch touch;   // default address 0x5A, uses Wire

void onTouch(uint8_t electrode, MPR121Event event) {
  if (event == MPR121Event::SHORT_CLICK)
    Serial.printf("Short click on E%d\n", electrode);

  if (event == MPR121Event::LONG_PRESS)
    Serial.printf("Long press on E%d\n", electrode);
}

void setup() {
  Serial.begin(115200);

  touch.setDebounceMs(20);
  touch.setShortClickMs(400);
  touch.setLongPressMs(800);
  touch.onEvent(onTouch);

  if (!touch.begin())
    Serial.println("MPR121 not found!");
}

void loop() {
  touch.update();   // call as often as possible
  delay(10);
}
```

---

## API Reference

### Constructor

```cpp
MPR121Touch touch(uint8_t addr = 0x5A, TwoWire* wire = &Wire);
```

### Initialisation

```cpp
bool begin();                        // returns false if chip not found
void setClockFrequency(uint32_t hz); // default 100 kHz; up to 400 kHz
```

### Timing (call before or after begin())

```cpp
void setDebounceMs(uint16_t ms);    // default 20
void setShortClickMs(uint16_t ms);  // default 400  – max press to count as click
void setLongPressMs(uint16_t ms);   // default 800  – min press to fire long-press
```

### Sensitivity

```cpp
void setThresholds(uint8_t touch = 12, uint8_t release = 6);
void setElectrodeThresholds(uint8_t electrode, uint8_t touch, uint8_t release);
```
Lower `touch` value = more sensitive. `release` must always be < `touch`.

### Callback

```cpp
void onEvent(void (*cb)(uint8_t electrode, MPR121Event event));
```

`MPR121Event` values: `TOUCH`, `RELEASE`, `SHORT_CLICK`, `LONG_PRESS`

### Poll loop

```cpp
uint16_t update();   // call from loop(); returns bitmask of touched electrodes
```

### State queries

```cpp
bool     isTouched(uint8_t electrode) const;
uint16_t getTouchedMask() const;   // bits 0–11
```

### Misc

```cpp
bool softReset();
bool isConnected();
bool    writeReg(uint8_t reg, uint8_t val);
uint8_t readReg(uint8_t reg);
```

---

## Event timing diagram

```
Finger down ───────────────────────────────────────────── Finger up
             │                         │                  │
             TOUCH                     LONG_PRESS         RELEASE
             (immediate)               (at 800 ms)        (no SHORT_CLICK
                                                           because long fired)

Finger down ──────── Finger up
             │       │       │
             TOUCH   RELEASE SHORT_CLICK
             (imm.)  (imm.)  (< 400 ms held)
```

---

## yoRadio integration

See `examples/yoRadioIntegration/` for a ready-to-use mapping of all 12 electrodes to yoRadio actions (play/pause, volume, next/previous station, mute, etc.).

---

## License

MIT
