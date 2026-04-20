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

#if L10N_LANGUAGE == PL
#include "tools/polishChars.h"
#endif

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

#if L10N_LANGUAGE == PL
  // Load Polish custom characters into CGRAM
  createChar(0, (uint8_t*)char_a_ogonek);  // ą
  createChar(1, (uint8_t*)char_c_acute);   // ć
  createChar(2, (uint8_t*)char_e_ogonek);  // ę
  createChar(3, (uint8_t*)char_l_stroke);  // ł
  createChar(4, (uint8_t*)char_n_acute);   // ń
  createChar(5, (uint8_t*)char_o_acute);   // ó
  createChar(6, (uint8_t*)char_s_acute);   // ś
  createChar(7, (uint8_t*)char_z_dot);     // ż
#endif

  clearClipping();
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

void DspCore::clearDsp(bool black){ clear(); }
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
    // Update sound meter on line 2 (line 1 has the clock)
    // Get display width
    static uint8_t lastSecond = 0xFF;
    #if defined(LCD_4002)
      const uint8_t displayWidth = 40;
      const uint8_t halfWidth = 20;
    #elif defined(LCD_2004) || defined(LCD_2002)
      const uint8_t displayWidth = 20;
      const uint8_t halfWidth = 10;
    #else
      const uint8_t displayWidth = 16;
      const uint8_t halfWidth = 8;
    #endif
    
    // Update cadence tuned for HD44780 over I2C (prioritize smoothness)
    const uint32_t now = millis();
    const uint16_t updateIntervalMs = (uint16_t)(1000 / 15); // slightly under 24 FPS.
    if (_soundMeterLastUpdate != 0 && (now - _soundMeterLastUpdate) < updateIntervalMs) {
        return;
    }
    _soundMeterLastUpdate = now;

    // Get audio levels
    uint16_t vulevel = player.getVUlevel();
    uint8_t rawL = (vulevel >> 8) & 0xFF;
    uint8_t rawR = vulevel & 0xFF;

    // Dynamic auto-scaling — envelope values are instantaneous so they swing widely.
    uint8_t framePeak = max(rawL, rawR);
    if (framePeak > _soundMeterAutoPeak) {
        _soundMeterAutoPeak = framePeak;
    } else {
        // Slow decay keeps the scale stable across a song section.
        const uint8_t autoPeakMin = 60;
        if (_soundMeterAutoPeak > autoPeakMin) {
            _soundMeterAutoPeak--;
        }
    }

    uint8_t scaleTop = max<uint8_t>(_soundMeterAutoPeak, static_cast<uint8_t>(80));

    // Simple linear map — the envelope values already swing naturally.
    auto mapLevel = [&](uint8_t raw) -> uint8_t {
        const uint8_t noiseFloor = 4;
        if (raw <= noiseFloor) return 0;

        uint8_t top = (scaleTop > noiseFloor) ? (scaleTop - noiseFloor) : 1;
        uint16_t scaled = map(raw - noiseFloor, 0, top, 0, halfWidth);

        if (scaled > halfWidth) scaled = halfWidth;
        return static_cast<uint8_t>(scaled);
    };

    uint8_t L = mapLevel(rawL);
    uint8_t R = mapLevel(rawR);
    
    bool played = player.isRunning();
    
    if(played) {
        // Fast attack, moderate decay — lets beats punch through
        if (L >= _soundMeterMeasL) _soundMeterMeasL = L;  // instant attack
        else _soundMeterMeasL = (_soundMeterMeasL > 1 ? _soundMeterMeasL - 1 : 0);

        if (R >= _soundMeterMeasR) _soundMeterMeasR = R;  // instant attack
        else _soundMeterMeasR = (_soundMeterMeasR > 1 ? _soundMeterMeasR - 1 : 0);
    } else {
        const uint8_t fadeRate = 2;
        if(_soundMeterMeasL > 0) _soundMeterMeasL = (_soundMeterMeasL > fadeRate) ? _soundMeterMeasL - fadeRate : 0;
        if(_soundMeterMeasR > 0) _soundMeterMeasR = (_soundMeterMeasR > fadeRate) ? _soundMeterMeasR - fadeRate : 0;
    }
    
    if(_soundMeterMeasL > halfWidth) _soundMeterMeasL = halfWidth;
    if(_soundMeterMeasR > halfWidth) _soundMeterMeasR = halfWidth;

    // Peak hold marker with short hold then decay
    const uint16_t peakHoldMs = 25;
    if (_soundMeterMeasL >= _soundMeterPeakL) {
        _soundMeterPeakL = _soundMeterMeasL;
        _soundMeterPeakHoldUntilL = now + peakHoldMs;
    } else if (now > _soundMeterPeakHoldUntilL && _soundMeterPeakL > 0) {
        _soundMeterPeakL--;
    }

    if (_soundMeterMeasR >= _soundMeterPeakR) {
        _soundMeterPeakR = _soundMeterMeasR;
        _soundMeterPeakHoldUntilR = now + peakHoldMs;
    } else if (now > _soundMeterPeakHoldUntilR && _soundMeterPeakR > 0) {
        _soundMeterPeakR--;
    }
    
    // Build sound meter line based on display width
    // Left channel: from left edge (0) towards center
    // Right channel: from right edge towards center
    #if defined(LCD_4002)
      char line[41]; // 40 chars + null
    #elif defined(LCD_2004) || defined(LCD_2002)
      char line[21]; // 20 chars + null
    #else
      char line[17]; // 16 chars + null
    #endif
    
    memset(line, ' ', displayWidth);
    
    // Fill left channel (from left)
    for(uint8_t i = 0; i < _soundMeterMeasL; i++) {
        line[i] = (char)0xFF; // Solid block character
    }
    
    // Fill right channel (from right)
    for(uint8_t i = 0; i < _soundMeterMeasR; i++) {
        line[displayWidth - 1 - i] = (char)0xFF; // Solid block character
    }

    // Draw peak markers when outside the filled body
    if (_soundMeterPeakL > 0 && _soundMeterPeakL <= halfWidth) {
        uint8_t peakPosL = _soundMeterPeakL - 1;
        if (line[peakPosL] == ' ') {
            line[peakPosL] = '|';
        }
    }
    if (_soundMeterPeakR > 0 && _soundMeterPeakR <= halfWidth) {
        uint8_t peakPosR = displayWidth - _soundMeterPeakR;
        if (line[peakPosR] == ' ') {
            line[peakPosR] = '|';
        }
    }
    
    line[displayWidth] = '\0';
    
    // Display only changed segments on line 2 to reduce I2C traffic
    if (_soundMeterPrevLine[0] == '\0') {
        setCursor(0, 1);
        print(line);
        strcpy(_soundMeterPrevLine, line);
    } else if (strcmp(_soundMeterPrevLine, line) != 0) {
        uint8_t start = 0;
        while (start < displayWidth) {
            while (start < displayWidth && _soundMeterPrevLine[start] == line[start]) {
                start++;
            }
            if (start >= displayWidth) break;

            uint8_t end = start;
            while (end < displayWidth && _soundMeterPrevLine[end] != line[end]) {
                end++;
            }

            char segment[41];
            uint8_t len = end - start;
            memcpy(segment, line + start, len);
            segment[len] = '\0';
            setCursor(start, 1);
            print(segment);
            start = end;
        }
        strcpy(_soundMeterPrevLine, line);
    }
    
    // Also update clock periodically
    if (network.timeinfo.tm_sec != lastSecond) {
        lastSecond = network.timeinfo.tm_sec;
        showSoundMeterClock(clockConf);
    }
}

#endif
