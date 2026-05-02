#include "../core/options.h"
#if DSP_MODEL==DSP_1602I2C || DSP_MODEL==DSP_1602 || DSP_MODEL==DSP_2004 || DSP_MODEL==DSP_2004I2C || DSP_MODEL==DSP_2002 || DSP_MODEL==DSP_2002I2C || DSP_MODEL==DSP_4002I2C
#include "dspcore.h"
#include <WiFi.h>
#include "../core/config.h"
#include "../core/network.h"
#include "../core/player.h"
#include "animations.h"
#include "tools/commongfx.h"
#include "conf/displayLCD1602conf.h"

// CGRAM custom chars (slots 1-6; slot 0 is avoided — it is \0 in C strings)
// Slot 1: speaker pointing right  (used at left edge of L-bar)
// Slot 2: speaker pointing left   (used at right edge of R-bar)
// Slots 3-6: smooth bar fill 1/5 .. 4/5 column  (0xFF = 5/5 full block)
static const uint8_t cgram_speaker_r[8] = { // slot 1 ▷ speaker right
  0b00001,
  0b00011,
  0b11101,
  0b11001,
  0b11101,
  0b00011,
  0b00001,
  0b00000
};
static const uint8_t cgram_speaker_l[8] = { // slot 2 ◁ speaker left
  0b10000,
  0b11000,
  0b10111,
  0b10011,
  0b10111,
  0b11000,
  0b10000,
  0b00000
};
static const uint8_t cgram_bar1[8] = { // slot 3  █ 1/5
  0b10000, 0b10000, 0b10000, 0b10000,
  0b10000, 0b10000, 0b10000, 0b00000
};
static const uint8_t cgram_bar2[8] = { // slot 4  ██ 2/5
  0b11000, 0b11000, 0b11000, 0b11000,
  0b11000, 0b11000, 0b11000, 0b00000
};
static const uint8_t cgram_bar3[8] = { // slot 5  ███ 3/5
  0b11100, 0b11100, 0b11100, 0b11100,
  0b11100, 0b11100, 0b11100, 0b00000
};
static const uint8_t cgram_bar4[8] = { // slot 6  ████ 4/5
  0b11110, 0b11110, 0b11110, 0b11110,
  0b11110, 0b11110, 0b11110, 0b00000
};
// 5/5 full block uses the LCD's native 0xFF character (no CGRAM needed)


#ifndef SCREEN_ADDRESS
  #define SCREEN_ADDRESS 0x27 ///< See datasheet for Address or scan it https://create.arduino.cc/projecthub/abdularbi17/how-to-scan-i2c-address-in-arduino-eaadda
#endif

// Static instance of animation controller
static LCDAnimationController lcdAnimController;

// LCDAnimationController implementation
LCDAnimationController::LCDAnimationController() {
  _currentFrame = 0;
  _totalFrames = 0;
  _lastUpdate = 0;
  _frameDuration = 0;
  _currentAnimation = ANIM_FISH;
  _animData = nullptr;
}

void LCDAnimationController::begin(AnimationType type) {
  _currentAnimation = type;
  _currentFrame = 0;
  _lastUpdate = millis();
  
  // Select appropriate animation based on display width
  #if DSP_MODEL==DSP_4002I2C
    // 40x2 display
    for(const auto& anim : animations40) {
      if(anim.type == type) {
        _animData = &anim;
        _totalFrames = anim.frameCount;
        _frameDuration = anim.frameDuration;
        break;
      }
    }
  #else
    // 20x2 or 16x2 displays
    for(const auto& anim : animations) {
      if(anim.type == type) {
        _animData = &anim;
        _totalFrames = anim.frameCount;
        _frameDuration = anim.frameDuration;
        break;
      }
    }
  #endif
}

bool LCDAnimationController::needsUpdate() {
  return (millis() - _lastUpdate >= _frameDuration);
}

void LCDAnimationController::update() {
  if(needsUpdate() && _animData != nullptr) {
    _currentFrame = (_currentFrame + 1) % _totalFrames;
    _lastUpdate = millis();
  }
}

const AnimFrame* LCDAnimationController::getCurrentFrame() {
  if(_animData == nullptr || _animData->frames == nullptr) return nullptr;
  return &(_animData->frames[_currentFrame]);
}

DspCore::DspCore(): DSP_INIT {
  _soundMeterMode = false;
  _soundMeterLastUpdate = 0;
  _soundMeterMeasL = 0;
  _soundMeterMeasR = 0;
  _soundMeterPeakL = 0;
  _soundMeterPeakR = 0;
  _soundMeterPeakHoldUntilL = 0;
  _soundMeterPeakHoldUntilR = 0;
  _soundMeterAutoPeak = 80;
  _soundMeterPrevLine[0] = '\0';
  _soundMeterPrevClockLine[0] = '\0';
  _soundMeterVUMeterWasEnabled = false;
}

void DspCore::apScreen() {
  clear();
  setCursor(0,0);
  print(utf8Rus(const_lcdApMode, false));
  setCursor(0,1);
  print(config.ipToStr(WiFi.softAPIP()));
#ifdef LCD_2004
  setCursor(0, 2);
  print(utf8Rus(const_lcdApName, false));
  print(apSsid);
  setCursor(0, 3);
  print(utf8Rus(const_lcdApPass, false));
  print(apPassword);
#endif
}

void DspCore::initDisplay() {
#ifdef LCD_I2C
  init();
  #ifdef LCD_4002
    Wire.setClock(400000);
  #endif
  backlight();
#else
  #ifdef LCD_2004
    begin(20, 4);
  #elif DSP_MODEL==DSP_2002 || DSP_MODEL==DSP_2002I2C
    begin(20, 2);
  #else
    begin(16, 2);
  #endif
#endif

// Load VU-meter custom characters into CGRAM slots 1-6.
// Slot 0 is intentionally skipped (it maps to '\0' in C strings).
// Slot 7 is left free for future use; native 0xFF = full-block bar.
_loadCGRAM();

  clearClipping();
}

void DspCore::_loadCGRAM() {
  createChar(1, (uint8_t*)cgram_speaker_r); // speaker → (L-bar left edge)
  createChar(2, (uint8_t*)cgram_speaker_l); // speaker ← (R-bar right edge)
  createChar(3, (uint8_t*)cgram_bar1);      // 1/5 partial column
  createChar(4, (uint8_t*)cgram_bar2);      // 2/5 partial column
  createChar(5, (uint8_t*)cgram_bar3);      // 3/5 partial column
  createChar(6, (uint8_t*)cgram_bar4);      // 4/5 partial column
}

void DspCore::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
  if(w<2) return;
  char buf[width()+1] = { 0 };
  snprintf(buf, sizeof(buf), "%*s%s", w-1, "", " ");
  setCursor(x, y);
  print(buf);
  setCursor(x, y);
}

uint16_t DspCore::width(){
#if defined(LCD_2004) || DSP_MODEL==DSP_2002 || DSP_MODEL==DSP_2002I2C
  return 20;
#elif DSP_MODEL==DSP_4002I2C
	return 40;
#else
  return 16;
#endif
}

uint16_t DspCore::height(){
#ifdef LCD_2004
  return 4;
#else
  return 2;
#endif
}

void DspCore::clearDsp(bool black){
  clear();
  // Reload CGRAM: I2C noise from GPIO switching (especially the 2-pulse
  // AUX->RADIO transition) can corrupt custom-character slots 1-6.
  _loadCGRAM();
  // Reset sound-meter diff state so stale VU icon bytes (\x01-\x06)
  // cannot bleed into the next widget render pass.
  _soundMeterPrevLine[0] = '\0';
  _soundMeterPrevClockLine[0] = '\0';
  _soundMeterMode = false;
}
void DspCore::flip(){ }
void DspCore::invert(){ }
void DspCore::sleep(void) { 
  noDisplay();
#ifdef LCD_I2C
  noBacklight();
#endif
}
void DspCore::wake(void) { 
  display();
#ifdef LCD_I2C
  backlight();
#endif
}

// Animation methods for screensaver
void DspCore::showAnimationFrame(const AnimFrame* frame) {
    if (frame == nullptr) return;

    static char prevLine1[41] = "";
    static char prevLine2[41] = "";
    
    char line1[41];
    char line2[41];
    strcpy(line1, frame->line1);
    strcpy(line2, frame->line2);
    
    // Replace time placeholder
    char timeBuf[6];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &network.timeinfo);
    char* pos = strstr(line1, "HH:MM");
    if (pos) {
        memcpy(pos, timeBuf, 5);
    }
    pos = strstr(line2, "HH:MM");
    if (pos) {
        memcpy(pos, timeBuf, 5);
    }

    // Replace date placeholder
    char dateBuf[11];
    strftime(dateBuf, sizeof(dateBuf), "%d/%m/%Y", &network.timeinfo);
    pos = strstr(line1, "DD/MM/YYYY");
    if (pos) {
        memcpy(pos, dateBuf, 10);
    }
    pos = strstr(line2, "DD/MM/YYYY");
    if (pos) {
        memcpy(pos, dateBuf, 10);
    }

    // Pad lines to full display width to clear old content
    uint16_t displayWidth = width();
    int len1 = strlen(line1);
    int len2 = strlen(line2);
    
    // Pad with spaces to full width
    for(int i = len1; i < displayWidth; i++) {
        line1[i] = ' ';
    }
    line1[displayWidth] = '\0';
    
    for(int i = len2; i < displayWidth; i++) {
        line2[i] = ' ';
    }
    line2[displayWidth] = '\0';

    // Only update lines that have changed
    if (strcmp(line1, prevLine1) != 0) {
        setCursor(0, 0);
        print(line1);
        strcpy(prevLine1, line1);
        yield();  // Let ESP32 service watchdog and background tasks
    }
    
    if (strcmp(line2, prevLine2) != 0) {
        setCursor(0, 1);
        print(line2);
        strcpy(prevLine2, line2);
        yield();  // Let ESP32 service watchdog and background tasks
    }
}

void DspCore::initScreensaver(AnimationType type) {
    lcdAnimController.begin(type);
    
    if (type == ANIM_SOUND_METER) {
        // Initialize sound meter mode
        _soundMeterMode = true;
        _soundMeterLastUpdate = 0;
        _soundMeterMeasL = 0;
        _soundMeterMeasR = 0;
        _soundMeterPeakL = 0;
        _soundMeterPeakR = 0;
        _soundMeterPeakHoldUntilL = 0;
        _soundMeterPeakHoldUntilR = 0;
        _soundMeterAutoPeak = 80;
        _soundMeterPrevLine[0] = '\0';
        _soundMeterPrevClockLine[0] = '\0';
        
        // Enable vumeter so get_VUlevel() returns actual values
        // Store previous state to restore later
        _soundMeterVUMeterWasEnabled = config.store.vumeter;
        if (!config.store.vumeter) {
            config.store.vumeter = true;
        }
		
        // Show clock on line 1
        showSoundMeterClock(clockConf);
        // Clear line 2 for sound meter
        setCursor(0, 1);
        #if defined(LCD_4002)
          print("                                        "); // 40 spaces
        #elif defined(LCD_2004) || defined(LCD_2002)
          print("                    "); // 20 spaces
        #else
          print("                "); // 16 spaces
        #endif
    } else {
        // Restore vumeter state if we had changed it for sound meter
        if (_soundMeterMode && !_soundMeterVUMeterWasEnabled) {
            config.store.vumeter = false;
        }
        
        _soundMeterMode = false;
        // Show first frame immediately
        const AnimFrame* frame = lcdAnimController.getCurrentFrame();
        showAnimationFrame(frame);
    }
}

void DspCore::updateScreensaver() {
    if (_soundMeterMode) {
        // Update sound meter
        updateSoundMeter();
    } else {
        // Regular animation
        if (lcdAnimController.needsUpdate()) {
            lcdAnimController.update();
            const AnimFrame* frame = lcdAnimController.getCurrentFrame();
            showAnimationFrame(frame);
        }
    }
}

void DspCore::showSoundMeterClock(const WidgetConfig& config) {
    // Format time string
    char timeBuf[6]; // HH:MM + null terminator
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &network.timeinfo);

    uint16_t displayWidth = width();
    char line[41]; // Max 40 chars + null
    memset(line, ' ', displayWidth);
    line[displayWidth] = '\0';

    int timeLen = strlen(timeBuf);
    int pos = 0;

    // Calculate position based on alignment
    switch (config.align) {
    case WA_LEFT:
        pos = config.left;
        break;
    case WA_CENTER:
        pos = (displayWidth - timeLen) / 2;
        break;
    case WA_RIGHT:
        pos = displayWidth - timeLen - config.left;
        break;
    }

    // Ensure position is within bounds
    if (pos < 0) pos = 0;
    if (pos + timeLen > displayWidth) pos = displayWidth - timeLen;

    // Copy time into position
    memcpy(line + pos, timeBuf, timeLen);

    // Diff-update clock row to avoid full-line I2C rewrite spikes
    if (_soundMeterPrevClockLine[0] == '\0') {
        setCursor(0, config.top);
        print(line);
        strcpy(_soundMeterPrevClockLine, line);
        return;
    }

    if (strcmp(_soundMeterPrevClockLine, line) != 0) {
        uint8_t start = 0;
        while (start < displayWidth) {
            while (start < displayWidth && _soundMeterPrevClockLine[start] == line[start]) {
                start++;
            }
            if (start >= displayWidth) break;

            uint8_t end = start;
            while (end < displayWidth && _soundMeterPrevClockLine[end] != line[end]) {
                end++;
            }

            char segment[41];
            uint8_t len = end - start;
            memcpy(segment, line + start, len);
            segment[len] = '\0';
            setCursor(start, config.top);
            print(segment);
            start = end;
        }

        strcpy(_soundMeterPrevClockLine, line);
    }
}

void DspCore::updateSoundMeter() {
    // Line layout (40-col example, scales proportionally):
    //   L side: [spkR][===bar19chars===][         ]
    //   R side: [         ][===bar19chars===][spkL]
    // Both halves overlap in the same line[] array (they meet in the middle).
    // barWidth = halfWidth - 1  (one cell is the speaker icon)
    // Sub-character smooth fill: 5 horizontal pixel columns per char cell.
    //   CGRAM slot 1=spkR, 2=spkL, 3=1/5, 4=2/5, 5=3/5, 6=4/5, 0xFF=full

    static uint8_t lastSecond = 0xFF;

    #if defined(LCD_4002)
      const uint8_t displayWidth = 40;
      const uint8_t halfWidth    = 20;
    #elif defined(LCD_2004) || defined(LCD_2002)
      const uint8_t displayWidth = 20;
      const uint8_t halfWidth    = 10;
    #else
      const uint8_t displayWidth = 16;
      const uint8_t halfWidth    = 8;
    #endif
    const uint8_t barWidth = halfWidth - 1; // chars available for actual bar
    const uint16_t maxPx   = (uint16_t)barWidth * 5; // sub-pixel resolution

    const uint32_t now = millis();
    const uint16_t updateIntervalMs = (uint16_t)(1000 / 15);
    if (_soundMeterLastUpdate != 0 && (now - _soundMeterLastUpdate) < updateIntervalMs) {
        return;
    }
    _soundMeterLastUpdate = now;

    // --- Audio levels ---
    uint16_t vulevel = player.getVUlevel();
    uint8_t rawL = (vulevel >> 8) & 0xFF;
    uint8_t rawR = vulevel & 0xFF;

    // Dynamic auto-scaling
    uint8_t framePeak = max(rawL, rawR);
    if (framePeak > _soundMeterAutoPeak) {
        _soundMeterAutoPeak = framePeak;
    } else {
        const uint8_t autoPeakMin = 60;
        if (_soundMeterAutoPeak > autoPeakMin) _soundMeterAutoPeak--;
    }
    uint8_t scaleTop = max<uint8_t>(_soundMeterAutoPeak, (uint8_t)80);

    // Map raw value to sub-pixel range 0..maxPx
    auto mapLevel = [&](uint8_t raw) -> uint16_t {
        const uint8_t noiseFloor = 4;
        if (raw <= noiseFloor) return 0;
        uint8_t top = (scaleTop > noiseFloor) ? (scaleTop - noiseFloor) : 1;
        uint32_t scaled = (uint32_t)(raw - noiseFloor) * maxPx / top;
        if (scaled > maxPx) scaled = maxPx;
        return (uint16_t)scaled;
    };

    uint16_t targetLpx = mapLevel(rawL);
    uint16_t targetRpx = mapLevel(rawR);

    bool played = player.isRunning();

    // Bar follows the instantaneous level directly — no hold-on-attack.
    // A gentle sub-pixel decay gives a brief tail so short spikes don't vanish instantly.
    // This keeps the bar "alive" and moving even during loud sustained passages.
    const uint16_t decayRate = 8; // sub-pixels per frame (~8/15 s to floor at full decay)
    if (played) {
        _soundMeterMeasL = targetLpx > _soundMeterMeasL
            ? targetLpx
            : (_soundMeterMeasL > decayRate ? _soundMeterMeasL - decayRate : 0);
        _soundMeterMeasR = targetRpx > _soundMeterMeasR
            ? targetRpx
            : (_soundMeterMeasR > decayRate ? _soundMeterMeasR - decayRate : 0);
    } else {
        // Fast drop when stopped
        const uint16_t stopFade = 15;
        _soundMeterMeasL = (_soundMeterMeasL > stopFade) ? _soundMeterMeasL - stopFade : 0;
        _soundMeterMeasR = (_soundMeterMeasR > stopFade) ? _soundMeterMeasR - stopFade : 0;
    }

    if (_soundMeterMeasL > maxPx) _soundMeterMeasL = maxPx;
    if (_soundMeterMeasR > maxPx) _soundMeterMeasR = maxPx;

    // Peak dot tracks the bar in char units; holds 1.5 s then slides down.
    uint8_t charL = (uint8_t)(_soundMeterMeasL / 5);
    uint8_t charR = (uint8_t)(_soundMeterMeasR / 5);

    const uint16_t peakHoldMs = 500;
    if (charL >= _soundMeterPeakL) {
        _soundMeterPeakL = charL;
        _soundMeterPeakHoldUntilL = now + peakHoldMs;
    } else if (now > _soundMeterPeakHoldUntilL && _soundMeterPeakL > 0) {
        _soundMeterPeakL--;
    }
    if (charR >= _soundMeterPeakR) {
        _soundMeterPeakR = charR;
        _soundMeterPeakHoldUntilR = now + peakHoldMs;
    } else if (now > _soundMeterPeakHoldUntilR && _soundMeterPeakR > 0) {
        _soundMeterPeakR--;
    }

    // --- Build line[] ---
    // Encoding: 0x01=spkR 0x02=spkL 0x03-0x06=bar1-4 0xFF=barFull ' '=empty '|'=peak
    char line[41];
    memset(line, ' ', displayWidth);
    line[displayWidth] = '\0';

    // Left speaker icon at position 0
    line[0] = (char)0x01;
    // Right speaker icon at last position
    line[displayWidth - 1] = (char)0x02;

    // L bar fills positions 1..barWidth (left to right)
    uint16_t Lpx = _soundMeterMeasL;
    for (uint8_t i = 0; i < barWidth; i++) {
        uint16_t cellPx = (uint16_t)(i + 1) * 5; // pixels consumed when cell i is full
        uint8_t pos = 1 + i;
        if (Lpx >= cellPx) {
            line[pos] = (char)0xFF;       // full block
        } else if (Lpx > cellPx - 5) {
            uint8_t frac = (uint8_t)(Lpx - (cellPx - 5)); // 1..4
            line[pos] = (char)(0x02 + frac); // slots 3..6
        }
        // else stays ' '
    }

    // R bar fills positions (displayWidth-2)..(displayWidth-1-barWidth) (right to left)
    uint16_t Rpx = _soundMeterMeasR;
    for (uint8_t i = 0; i < barWidth; i++) {
        uint16_t cellPx = (uint16_t)(i + 1) * 5;
        uint8_t pos = displayWidth - 2 - i;
        if (line[pos] != ' ') break; // don't overwrite L bar if channels overlap
        if (Rpx >= cellPx) {
            line[pos] = (char)0xFF;
        } else if (Rpx > cellPx - 5) {
            uint8_t frac = (uint8_t)(Rpx - (cellPx - 5));
            line[pos] = (char)(0x02 + frac);
        }
    }

    // Peak markers — only draw outside the filled body
    if (_soundMeterPeakL > 0 && _soundMeterPeakL <= barWidth) {
        uint8_t pp = _soundMeterPeakL; // 1-based from left (after speaker)
        if (line[pp] == ' ') line[pp] = '|';
    }
    if (_soundMeterPeakR > 0 && _soundMeterPeakR <= barWidth) {
        uint8_t pp = displayWidth - 1 - _soundMeterPeakR; // mirror
        if (line[pp] == ' ') line[pp] = '|';
    }

    // --- Diff-update line 2 ---
    if (_soundMeterPrevLine[0] == '\0') {
        setCursor(0, 1);
        print(line);
        memcpy(_soundMeterPrevLine, line, displayWidth + 1);
    } else if (memcmp(_soundMeterPrevLine, line, displayWidth) != 0) {
        uint8_t start = 0;
        while (start < displayWidth) {
            while (start < displayWidth && _soundMeterPrevLine[start] == line[start]) start++;
            if (start >= displayWidth) break;
            uint8_t end = start;
            while (end < displayWidth && _soundMeterPrevLine[end] != line[end]) end++;
            char seg[41];
            uint8_t len = end - start;
            memcpy(seg, line + start, len);
            seg[len] = '\0';
            setCursor(start, 1);
            print(seg);
            start = end;
        }
        memcpy(_soundMeterPrevLine, line, displayWidth + 1);
    }

    // Update clock on line 0 every second
    if (network.timeinfo.tm_sec != lastSecond) {
        lastSecond = network.timeinfo.tm_sec;
        showSoundMeterClock(clockConf);
    }
}

#endif
