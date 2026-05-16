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
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// ---------------------------------------------------------------------------
// LCD write-offload — dual-core, coherent column rendering.
//
// Two item types share the queue:
//
//   SPECTRUM_FRAME — one whole spectrum frame. Core 1 writes the full bar area
//                    in one burst, so right-side columns are not older than the
//                    left side by a queue of per-column items.
//
//   WRITE_BUF      — arbitrary run (clock, station name). Core 1 does a normal
//                    setCursor + sequential write.
//
// Core 0 (audio/WiFi) only enqueues items and returns in ~1 µs per item.
// Core 1 owns all I2C transactions.
// ---------------------------------------------------------------------------
struct LcdCmd {
    enum Type : uint8_t { SPECTRUM_FRAME, WRITE_BUF } type;
    uint8_t col;
    uint8_t row;       // used by WRITE_BUF
    uint8_t barCols;   // SPECTRUM_FRAME: row-0 active width
    uint8_t stBarCols; // SPECTRUM_FRAME: row-1 active width
    uint8_t len;       // WRITE_BUF: number of bytes
    uint64_t dirtyMask; // SPECTRUM_FRAME: bit N set = column N changed (max 40 cols)
    char    buf[41];   // WRITE_BUF: payload / row-0 frame data
    char    buf2[41];  // SPECTRUM_FRAME: row-1 frame data
};

static QueueHandle_t _lcdQueue     = nullptr; // depth-1 mailbox for SPECTRUM_FRAME (xQueueOverwrite)
static QueueHandle_t _lcdTextQueue = nullptr; // depth-8 fifo for WRITE_BUF (clock, RDS, station name)
static TaskHandle_t  _lcdTask  = nullptr;
static SemaphoreHandle_t _lcdMutex = nullptr; // guards all direct LCD (I2C) access
static volatile bool _lcdDropEnqueue = false; // true during direct-mode screen transitions
static volatile bool _lcdAsyncSpectrum = false; // queue writes only while spectrum mode is active

// Flush the queue and acquire the LCD mutex from Core 0.
// Call before any direct LCD operation (clear, createChar, etc.).
static void lcdAcquire() {
    _lcdAsyncSpectrum = false;
    _lcdDropEnqueue = true;
    if (_lcdMutex) xSemaphoreTake(_lcdMutex, portMAX_DELAY);
    if (_lcdQueue)     xQueueReset(_lcdQueue);     // drop stale spectrum frames
    if (_lcdTextQueue) xQueueReset(_lcdTextQueue); // drop stale text writes
}

static void lcdRelease() {
    _lcdDropEnqueue = false;
    if (_lcdMutex) xSemaphoreGive(_lcdMutex);
}

static void lcdWriteTask(void* arg) {
    DspCore* d = static_cast<DspCore*>(arg);
    LcdCmd cmd;
    for (;;) {
        if (xQueueReceive(_lcdQueue, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) {
            vTaskDelay(1);
            continue;
        }
        xSemaphoreTake(_lcdMutex, portMAX_DELAY);
        if (cmd.type == LcdCmd::SPECTRUM_FRAME) {
            // --- Write row0 (top) FIRST, then row1 (bottom) ---
            // Rule: if a bar is not tall enough to fill the top cell, the top
            // must be cleared BEFORE the bottom is updated. Writing top-first
            // means a falling bar loses its top before its bottom grows, which
            // avoids the "floating top" detachment artifact.
            //
            // We scan the dirtyMask and batch consecutive dirty columns into a
            // single setCursor+write burst to minimise I2C transactions.

            // Pass 1: row 0 (top half) — full bar area (barCols)
            {
                uint8_t i = 0;
                while (i < cmd.barCols) {
                    // skip clean columns
                    while (i < cmd.barCols && !(cmd.dirtyMask & (1ULL << i))) i++;
                    if (i >= cmd.barCols) break;
                    uint8_t start = i;
                    // extend run while columns are dirty AND contiguous
                    while (i < cmd.barCols && (cmd.dirtyMask & (1ULL << i))) i++;
                    d->setCursor(start, 0);
                    for (uint8_t c = start; c < i; c++) d->write((uint8_t)cmd.buf[c]);
                }
            }

            // Pass 2: row 1 (bottom half) — bar area only (stBarCols)
            {
                uint8_t i = 0;
                while (i < cmd.stBarCols) {
                    while (i < cmd.stBarCols && !(cmd.dirtyMask & (1ULL << i))) i++;
                    if (i >= cmd.stBarCols) break;
                    uint8_t start = i;
                    while (i < cmd.stBarCols && (cmd.dirtyMask & (1ULL << i))) i++;
                    d->setCursor(start, 1);
                    for (uint8_t c = start; c < i; c++) d->write((uint8_t)cmd.buf2[c]);
                }
            }
        } else { // WRITE_BUF
            d->setCursor(cmd.col, cmd.row);
            for (uint8_t i = 0; i < cmd.len; i++) d->write((uint8_t)cmd.buf[i]);
        }
        xSemaphoreGive(_lcdMutex);

        // Drain all pending text items (clock blink, RDS, station name).
        // Done after the spectrum frame so text is never starved.
        if (_lcdTextQueue) {
            LcdCmd txt;
            while (xQueueReceive(_lcdTextQueue, &txt, 0) == pdTRUE) {
                xSemaphoreTake(_lcdMutex, portMAX_DELAY);
                txt.buf[txt.len] = '\0';
                d->setCursor(txt.col, txt.row);
                for (uint8_t i = 0; i < txt.len; i++) d->write((uint8_t)txt.buf[i]);
                xSemaphoreGive(_lcdMutex);
            }
        }
        vTaskDelay(1);
    }
}

// Enqueue one full spectrum frame so all columns stay temporally aligned.
// Uses xQueueOverwrite so Core 1 always renders the *newest* snapshot —
// no backlog, no stale-frame lag. dirtyMask bit N = column N has changed.
static inline void lcdQueueSpectrumFrame(const uint8_t* row0, const uint8_t* row1,
                                          uint8_t barCols, uint8_t stBarCols,
                                          uint64_t dirtyMask) {
    if (!_lcdQueue || !_lcdAsyncSpectrum || _lcdDropEnqueue) return;
    LcdCmd cmd{};
    cmd.type      = LcdCmd::SPECTRUM_FRAME;
    cmd.barCols   = barCols;
    cmd.stBarCols = stBarCols;
    cmd.dirtyMask = dirtyMask;
    memcpy(cmd.buf,  row0, barCols);
    memcpy(cmd.buf2, row1, stBarCols);
    xQueueOverwrite(_lcdQueue, &cmd); // always succeeds, overwrites stale frame if queued
}

// Enqueue an arbitrary run (clock, station name, RDS) to the dedicated text queue.
// This queue is separate from the spectrum mailbox so xQueueOverwrite on spectrum
// frames can never clobber a clock or RDS item.
static inline bool lcdQueuePrint(uint8_t col, uint8_t row, const char* s, uint8_t len) {
    if (!_lcdTextQueue || !_lcdAsyncSpectrum || _lcdDropEnqueue || len == 0 || len > 40) return false;
    LcdCmd cmd{};
    cmd.type = LcdCmd::WRITE_BUF;
    cmd.col  = col; cmd.row = row; cmd.len = len;
    memcpy(cmd.buf, s, len);
    return xQueueSend(_lcdTextQueue, &cmd, 0) == pdTRUE; // non-blocking: drop if full
}

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

// Spectrum analyser vertical-fill CGRAM chars (slots 1-7).
// Each character fills N rows from the BOTTOM of the 8-row cell (all 5 columns lit).
// Slot 0 avoided (\0 in C strings). 0xFF = built-in full block (8/8).
// Encoding: bar height 0..16 across two LCD rows.
//   height 0     -> row0=' '  row1=' '
//   height 1..8  -> row0=' '  row1=slot[height]  (slot8=0xFF)
//   height 9..16 -> row0=slot[height-8]           row1=0xFF
static const uint8_t cgram_vfill[7][8] = {
  { 0,0,0,0,0,0,0,0b11111 }, // slot 1 — 1 row filled from bottom
  { 0,0,0,0,0,0,0b11111,0b11111 }, // slot 2 — 2 rows
  { 0,0,0,0,0,0b11111,0b11111,0b11111 }, // slot 3 — 3 rows
  { 0,0,0,0,0b11111,0b11111,0b11111,0b11111 }, // slot 4 — 4 rows
  { 0,0,0,0b11111,0b11111,0b11111,0b11111,0b11111 }, // slot 5 — 5 rows
  { 0,0,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111 }, // slot 6 — 6 rows
  { 0,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111 }, // slot 7 — 7 rows
};
// slot mapping helper: height 1-7 -> cgram slot 1-7; height 8 -> 0xFF
static inline char vfillChar(uint8_t h) {
  if (h == 0) return ' ';
  if (h >= 8) return (char)0xFF;
  return (char)h; // slots 1-7 loaded into CGRAM positions 1-7
}

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
  _spectrumMode = false;
  _spectrumLastUpdate = 0;
  _spectrumAutoPeak = 60;
  memset(_spectrumMeas, 0, sizeof(_spectrumMeas));
  memset(_spectrumPeak, 0, sizeof(_spectrumPeak));
  memset(_spectrumPeakHold, 0, sizeof(_spectrumPeakHold));
  _spectrumPrevRow0[0] = '\0';
  _spectrumPrevRow1[0] = '\0';
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
  Wire.setClock(800000); // 800 kHz high-speed — some I2C LCD backpacks support this
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

  // Start Core-1 LCD write task (queued I2C offload)
  if (_lcdQueue == nullptr) {
    _lcdMutex     = xSemaphoreCreateMutex();
    _lcdQueue     = xQueueCreate(1, sizeof(LcdCmd)); // mailbox: newest spectrum frame wins
    _lcdTextQueue = xQueueCreate(8, sizeof(LcdCmd)); // fifo: clock / RDS / station text
    xTaskCreatePinnedToCore(lcdWriteTask, "lcdWrite", 2048, this, 2, &_lcdTask, 1);
  }

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

void DspCore::_loadSpectrumCGRAM() {
  // Slots 1-7: vertical fill 1/8 .. 7/8 rows from bottom (5 columns wide).
  // 0xFF = built-in full block (8/8) — no slot needed.
  for (uint8_t i = 0; i < 7; i++) {
    createChar(i + 1, (uint8_t*)cgram_vfill[i]);
  }
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
  lcdAcquire();
  clear();
  _loadCGRAM();
  lcdRelease();
  _soundMeterPrevLine[0] = '\0';
  _soundMeterPrevClockLine[0] = '\0';
  _soundMeterMode = false;
  _spectrumMode = false;
  _spectrumPrevRow0[0] = '\0';
  _spectrumPrevRow1[0] = '\0';
}
void DspCore::flip(){ }
void DspCore::invert(){ }
void DspCore::sleep(void) {
  lcdAcquire();
  noDisplay();
#ifdef LCD_I2C
  noBacklight();
#endif
  lcdRelease();
}
void DspCore::wake(void) {
  lcdAcquire();
  display();
#ifdef LCD_I2C
  backlight();
#endif
  lcdRelease();
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
        _lcdAsyncSpectrum = false;
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
		
        lcdAcquire();
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
        lcdRelease();
    } else if (type == ANIM_SPECTRUM) {
        // Restore horizontal sound meter state if switching away from it
        if (_soundMeterMode && !_soundMeterVUMeterWasEnabled) {
            config.store.vumeter = false;
        }
        _soundMeterMode = false;

        // Spectrum mode init
        _spectrumMode = true;
        _spectrumLastUpdate = 0;
        _spectrumAutoPeak = 60;
        memset(_spectrumMeas, 0, sizeof(_spectrumMeas));
        memset(_spectrumPeak, 0, sizeof(_spectrumPeak));
        memset(_spectrumPeakHold, 0, sizeof(_spectrumPeakHold));
        _spectrumPrevRow0[0] = '\0';
        _spectrumPrevRow1[0] = '\0';

        // Enable vumeter so getVUlevel() returns actual values
        _soundMeterVUMeterWasEnabled = config.store.vumeter;
        if (!config.store.vumeter) config.store.vumeter = true;

        // All direct LCD operations below must be serialised with the write task.
        lcdAcquire();

        // Load vertical-fill CGRAM set
        _loadSpectrumCGRAM();

        // Clear both rows then draw initial clock (row 0 right) and station name (row 1 right)
        {
            uint16_t w = width();
            char blank[41]; memset(blank, ' ', w); blank[w] = '\0';
            setCursor(0, 0); print(blank);
            setCursor(0, 1); print(blank);
        }
        {
            char timeBuf[6];
            strftime(timeBuf, sizeof(timeBuf), "%H:%M", &network.timeinfo);
            #if DSP_MODEL==DSP_4002I2C
            {
                // Row 0: clock at col 35
                setCursor(35, 0);
                print(timeBuf);
                // Row 0: station name at col 20 (14 chars, centered)
                const char* sname = config.station.name;
                uint8_t nameLen = (uint8_t)strnlen(sname, 64);
                char stBuf[15];
                memset(stBuf, ' ', 14);
                if (nameLen <= 14) {
                    uint8_t pad = (14 - nameLen) / 2;
                    memcpy(stBuf + pad, sname, nameLen);
                } else { memcpy(stBuf, sname, 14); }
                stBuf[14] = '\0';
                setCursor(20, 0);
                print(stBuf);
                // Row 1: RDS title at col 21 (19 chars)
                const char* rtitle = config.station.title;
                uint8_t rdsLen2 = (uint8_t)strnlen(rtitle, 64);
                char rdsBuf[20];
                memset(rdsBuf, ' ', 19);
                if (rdsLen2 > 0) memcpy(rdsBuf, rtitle, rdsLen2 < 19 ? rdsLen2 : 19);
                rdsBuf[19] = '\0';
                setCursor(21, 1);
                print(rdsBuf);
            }
            #else
            {
                const uint8_t barCols = (uint8_t)width() - 5;
                setCursor(barCols, 0);
                print(timeBuf);
            }
            #endif
        }

        lcdRelease();
        _lcdAsyncSpectrum = true;
    } else {
        _lcdAsyncSpectrum = false;
        // Restore vumeter state if it was changed for sound meter or spectrum
        if ((_soundMeterMode || _spectrumMode) && !_soundMeterVUMeterWasEnabled) {
            config.store.vumeter = false;
        }
        _soundMeterMode = false;
        _spectrumMode = false;
        lcdAcquire();
        // Reload horizontal-bar CGRAM so next sound meter entry works correctly
        _loadCGRAM();
        // Show first frame immediately
        const AnimFrame* frame = lcdAnimController.getCurrentFrame();
        showAnimationFrame(frame);
        lcdRelease();
    }
}

void DspCore::updateScreensaver() {
    if (_soundMeterMode) {
        updateSoundMeter();
    } else if (_spectrumMode) {
        updateSpectrum();
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

// ---------------------------------------------------------------------------
// Spectrum analyser — vertical bars across both LCD rows.
//
// Visual layout (all sizes):
//   row 0: [bar × barCols][ HH:MM (5) ]
//   row 1: [bar × barCols]
//
// Visual layout (40-col only):
//   row 0: [bars 0..19][ station name 14 ][ ][ HH:MM ]
//                       col 20            34  35
//   row 1: [bars 0..19][ ][ RDS / track title 19 chars scrolling ]
//                       20  21
//
// Each column = one spectrum band.
// Bar height 0-16 (2 rows × 8 pixel rows):
//   height 0     -> row0=' '  row1=' '
//   height 1-8   -> row0=' '  row1=vfillChar(height)
//   height 9-16  -> row0=vfillChar(height-8)  row1=0xFF
//
void DspCore::updateSpectrum() {
    static uint8_t lastSecond = 0xFF;

    #if defined(LCD_4002)
      const uint8_t displayWidth = 40;
    #elif defined(LCD_2004) || defined(LCD_2002)
      const uint8_t displayWidth = 20;
    #else
      const uint8_t displayWidth = 16;
    #endif

    const uint32_t now = millis();
    const uint16_t updateIntervalMs = 16; // ~60 fps — suits 400 kHz I2C budget
    if (_spectrumLastUpdate != 0 && (now - _spectrumLastUpdate) < updateIntervalMs) return;
    _spectrumLastUpdate = now;

    // Layout (all sizes):
    //   row 0: [bars 0..barCols-1][ clock 5 chars ]
    //   row 1: [bars 0..stBarCols-1][ gap + station name (40-col only) ]
    //
    // On 40-col the spectrum is capped at 20 columns so the right half of the
    // display is always free for station name / clock and other I2C devices get
    // regular bus gaps between bursts.
    const uint8_t clockLen  = 5;
    #if DSP_MODEL==DSP_4002I2C
    const uint8_t barCols   = 20;   // spectrum bar columns (both rows)
    const uint8_t stBarCols = 20;
    const uint8_t stNameLen = 14;   // station name: row 0 cols 20-33
    const uint8_t stNameCol = 20;
    const uint8_t clockCol  = 35;   // clock: row 0 cols 35-39 (col 34 = gap)
    const uint8_t rdsLen    = 19;   // RDS/title: row 1 cols 21-39 (col 20 = gap)
    const uint8_t rdsCol    = 21;
    #else
    const uint8_t barCols   = displayWidth - clockLen;
    const uint8_t stBarCols = barCols;
    const uint8_t clockCol  = barCols;
    #endif

    // --- Real FFT spectrum bands (one source band per visible column) ---
    const uint8_t fftBandCount = stBarCols;
    uint8_t fftBands[40] = {0};
    bool played = player.isRunning();
    if (played) {
        player.getSpectrumBands(fftBands, fftBandCount);
    }

    // Auto-peak uses the raw source bands directly.
    // Decay is time-gated so a transient loud peak doesn't immediately
    // collapse; but quiet passages let the scale contract fairly quickly.
    // Hard floor at 20 (instead of 40) so bars can fall low during silence.
    static uint32_t autoPeakDecayAt = 0;
    if (played) {
        uint8_t rawPeak = 0;
        for (uint8_t i = 0; i < fftBandCount; i++) if (fftBands[i] > rawPeak) rawPeak = fftBands[i];
        if (rawPeak > _spectrumAutoPeak) {
            _spectrumAutoPeak = rawPeak;
            autoPeakDecayAt = now + 500; // hold 0.5 s before allowing decay
        } else if (now >= autoPeakDecayAt && _spectrumAutoPeak > 60) {
            _spectrumAutoPeak -= 3; // 3 steps per 16 ms — snappy auto-scale collapse
        }
    }
    const uint8_t scaleTop = (_spectrumAutoPeak > 60) ? _spectrumAutoPeak : 60;

    // --- Compute new character codes (Core 0, pure computation, no I2C) ---
    uint8_t newR0[40], newR1[40];

    for (uint8_t b = 0; b < stBarCols; b++) {
        uint8_t raw = fftBands[b];

        // Scale against auto-peak so bars reach full 16-step range.
        // Subtract a small noise floor first so residual hiss doesn't
        // keep bars floating above zero during quiet passages.
        const uint8_t noiseFloor = 8;
        uint8_t rawClamped = (raw > noiseFloor) ? (raw - noiseFloor) : 0;
        uint8_t scaleRange = (scaleTop > noiseFloor) ? (scaleTop - noiseFloor) : 1;
        float targetH = ((float)rawClamped * 16.0f) / (float)scaleRange;
        if (targetH > 16.0f) targetH = 16.0f;

        // Float-domain attack/release smoothing.
        // This keeps internal motion high-resolution and only quantizes at draw time,
        // which looks much smoother than dropping integer bar heights by fixed steps.
        if (played) {
            const float current = _spectrumMeas[b];
            if (targetH >= current) {
                constexpr float ATTACK = 0.85f;
                _spectrumMeas[b] = current + ATTACK * (targetH - current);
            } else {
                constexpr float RELEASE_MIN = 0.18f;
                constexpr float RELEASE_MAX = 0.32f;
                float release = RELEASE_MIN + (current / 16.0f) * (RELEASE_MAX - RELEASE_MIN);
                _spectrumMeas[b] = current + release * (targetH - current);
            }
        } else {
            constexpr float STOP_RELEASE = 0.35f;
            _spectrumMeas[b] += STOP_RELEASE * (0.0f - _spectrumMeas[b]);
        }
        if (_spectrumMeas[b] < 0.0f) _spectrumMeas[b] = 0.0f;
        if (_spectrumMeas[b] > 16.0f) _spectrumMeas[b] = 16.0f;

        uint8_t h = (uint8_t)(_spectrumMeas[b] + 0.5f);
        if (h > 16) h = 16;

        // Peak dot tracking
        const uint8_t peakH = (h > 15) ? 15 : h;
        if (peakH >= _spectrumPeak[b]) {
            _spectrumPeak[b] = peakH;
            _spectrumPeakHold[b] = now + 600;
        } else if (now > _spectrumPeakHold[b] && _spectrumPeak[b] > 0) {
            _spectrumPeak[b]--;
        }

        // Encode into two characters
        uint8_t topChar, botChar;
        if (h == 0) {
            botChar = ' '; topChar = ' ';
        } else if (h <= 8) {
            botChar = (uint8_t)vfillChar(h); topChar = ' ';
        } else {
            botChar = 0xFF; topChar = (uint8_t)vfillChar(h - 8);
        }

        // Peak dot: one step above bar top, but NEVER in a row the bar doesn't reach.
        // Rule: if h <= 8, bar is wholly in the bottom row — peak must stay there too.
        // Putting a peak dot in the top row while the bar is in the bottom row creates
        // the "floating / detached top" artifact.
        uint8_t pk = _spectrumPeak[b];
        if (pk > h && pk > 0) {
            if (pk <= 8 || h <= 8) {
                // Peak stays in bottom row — cap it there
                uint8_t pkBot = (pk > 8) ? 8 : pk; // never exceed full-bottom
                if (botChar == ' ') botChar = (uint8_t)vfillChar(pkBot);
            } else {
                // Both bar and peak are in the top row
                if (topChar == ' ') topChar = (uint8_t)vfillChar(pk - 8);
            }
        }

        newR1[b] = botChar;
        newR0[b] = topChar;
    }

    // --- Build dirty mask
    //
    // Coherence rule: if column b has no top (newR0[b]==' ') then the top
    // character never appears. Both row0 and row1 are always written together
    // per column (via the dirty mask), so the LCD never sees a half-column
    // update. xQueueOverwrite guarantees Core 1 always renders the freshest
    // snapshot — no backlog, no right-side lag.

    const bool fresh = (_spectrumPrevRow0[0] == '\0');
    uint64_t dirtyMask = 0;

    // Spectrum bar columns
    for (uint8_t b = 0; b < stBarCols; b++) {
        if (fresh ||
            (uint8_t)_spectrumPrevRow0[b] != newR0[b] ||
            (uint8_t)_spectrumPrevRow1[b] != newR1[b]) {
            dirtyMask |= (1ULL << b);
        }
    }
    if (dirtyMask) {
        lcdQueueSpectrumFrame(newR0, newR1, barCols, stBarCols, dirtyMask);
        // Update prev state unconditionally: xQueueOverwrite always delivers the
        // frame, so the state will be on screen after the next Core-1 tick.
        memcpy(_spectrumPrevRow0, newR0, barCols);
        memcpy(_spectrumPrevRow1, newR1, stBarCols);
    }

    if (fresh) {
        _spectrumPrevRow0[barCols]   = '\0';
        _spectrumPrevRow1[stBarCols] = '\0';
    }

    // --- Clock blink every 500 ms (colon on/off); full HH:MM reformat every 10 s ---
    // Runs independently of the spectrum frame rate — no I2C cost on Core 0.
    {
        static uint32_t lastClockBlink = 0;
        static bool colonOn = true;

        if (now - lastClockBlink >= 500) {
            lastClockBlink = now;
            colonOn = !colonOn;

            char timeBuf[6];
            strftime(timeBuf, sizeof(timeBuf), "%H:%M", &network.timeinfo);
            if (!colonOn) timeBuf[2] = ' '; // blink the colon
            lcdQueuePrint(clockCol, 0, timeBuf, clockLen);
        }
    }

    #if DSP_MODEL==DSP_4002I2C
    // --- Station name: update once per second (scrolls if longer than 14 chars) ---
    if (network.timeinfo.tm_sec != lastSecond) {
        lastSecond = network.timeinfo.tm_sec;
        static uint8_t stScrollOffset = 0;
        static char prevStBuf[15] = {};
        const char* sname = config.station.name;
        uint8_t nameLen = (uint8_t)strnlen(sname, 128);
        char stBuf[15];
        memset(stBuf, ' ', stNameLen);
        if (nameLen <= stNameLen) {
            uint8_t pad = (stNameLen - nameLen) / 2;
            memcpy(stBuf + pad, sname, nameLen);
            stScrollOffset = 0;
        } else {
            memcpy(stBuf, sname + stScrollOffset, stNameLen);
            stScrollOffset = (stScrollOffset + 1 + stNameLen > nameLen) ? 0 : stScrollOffset + 1;
        }
        stBuf[stNameLen] = '\0';
        if (memcmp(prevStBuf, stBuf, stNameLen) != 0) {
            lcdQueuePrint(stNameCol, 0, stBuf, stNameLen);
            memcpy(prevStBuf, stBuf, stNameLen + 1);
        }
    }

    // --- RDS / track title: scroll every 300 ms, fully independent of clock ---
    {
        static uint32_t lastRdsScroll = 0;
        static uint8_t  rdsScrollOffset = 0;
        static char     prevRdsBuf[20] = {};
        static char     prevRdsSource[129] = {}; // reset scroll when title changes

        if (now - lastRdsScroll >= 300) {
            lastRdsScroll = now;

            const char* rtitle = config.station.title;
            uint8_t rdsTextLen = (uint8_t)strnlen(rtitle, 128);

            // Reset scroll position whenever the title text changes
            if (strncmp(prevRdsSource, rtitle, 128) != 0) {
                strncpy(prevRdsSource, rtitle, 128);
                prevRdsSource[128] = '\0';
                rdsScrollOffset = 0;
                prevRdsBuf[0] = '\0'; // force redraw
            }

            char rdsBuf[20];
            memset(rdsBuf, ' ', rdsLen);
            if (rdsTextLen > 0) {
                if (rdsTextLen <= rdsLen) {
                    memcpy(rdsBuf, rtitle, rdsTextLen);
                    rdsScrollOffset = 0;
                } else {
                    memcpy(rdsBuf, rtitle + rdsScrollOffset, rdsLen);
                    rdsScrollOffset = (rdsScrollOffset + 1 + rdsLen > rdsTextLen) ? 0 : rdsScrollOffset + 1;
                }
            }
            rdsBuf[rdsLen] = '\0';
            if (memcmp(prevRdsBuf, rdsBuf, rdsLen) != 0) {
                lcdQueuePrint(rdsCol, 1, rdsBuf, rdsLen);
                memcpy(prevRdsBuf, rdsBuf, rdsLen + 1);
            }
        }
    }
    #endif
}

#endif
