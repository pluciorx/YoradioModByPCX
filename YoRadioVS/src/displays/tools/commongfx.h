#ifndef common_gfx_h
#define common_gfx_h
#include "../widgets/widgetsconfig.h" // displayXXXDDDDconf.h
#include "utf8Rus.h"

// Forward declarations for LCD animations
#ifdef DSP_LCD
enum AnimationType : uint8_t;
struct AnimFrame;
#endif

typedef struct clipArea {
  uint16_t left; 
  uint16_t top; 
  uint16_t width;  
  uint16_t height;
} clipArea;

class psFrameBuffer;

class DspCore: public yoDisplay {
  public:
    DspCore();
    void initDisplay();
    void clearDsp(bool black=false);
    void printClock(){}
    #ifdef DSP_OLED
    inline void loop(bool force=false){
      #if DSP_MODEL==DSP_NOKIA5110
        if(digitalRead(TFT_CS)==LOW) return;
        display();
      #else
        display();
        //delay(DSP_MODEL==DSP_ST7920?20:5);
        vTaskDelay(DSP_MODEL==DSP_ST7920?10:0);
      #endif
    }
    inline void drawLogo(uint16_t top) {
      #if DSP_MODEL!=DSP_SSD1306x32
        drawBitmap((width()  - LOGO_WIDTH ) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT, 1);
      #else
        setTextSize(1); setCursor((width() - 6*CHARWIDTH) / 2, 0); setTextColor(TFT_FG, TFT_BG); print(utf8Rus("ёRadio", false));
      #endif
      display();
    }
    #else
      #ifndef DSP_LCD
      inline void loop(bool force=false){}
      inline void drawLogo(uint16_t top){ drawRGBBitmap((width() - LOGO_WIDTH) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT); }
      #endif
    #endif
    #ifdef DSP_LCD
      uint16_t width();
      uint16_t height();
      void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
      void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){}
      void setTextSize(uint8_t s){}
      void setTextSize(uint8_t sx, uint8_t sy){}
      void setTextColor(uint16_t c, uint16_t bg){}
      void setFont(){}
      void apScreen();
      void drawLogo(uint16_t top){}
      void loop(bool force=false){}
      // Screensaver animation methods
      void initScreensaver(AnimationType type);
      void updateScreensaver();
      void showAnimationFrame(const AnimFrame* frame);
      // Sound meter methods
      void updateSoundMeter();  
      void showSoundMeterClock(const WidgetConfig& config);
      void updateSpectrum();
    private:
      void _loadCGRAM();
      void _loadSpectrumCGRAM();
    public:
    #endif
    void flip();
    void invert();
    void sleep();
    void wake();
    void setScrollId(void * scrollid) { _scrollid = scrollid; }
    void * getScrollId() { return _scrollid; }
    uint16_t textWidth(const char *txt);
    #if !defined(DSP_LCD)
      inline void writePixel(int16_t x, int16_t y, uint16_t color) {
        if(_clipping){
          if ((x < _cliparea.left) || (x > _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height)) return;
        }
        yoDisplay::writePixel(x, y, color);
      }
      inline void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        if(_clipping){
          if ((x < _cliparea.left) || (x >= _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height))  return;
        }
        yoDisplay::writeFillRect(x, y, w, h, color);
      }
    #else
      inline void writePixel(int16_t x, int16_t y, uint16_t color) { }
      inline void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { }
    #endif
    inline void setClipping(clipArea ca){
      _cliparea = ca;
      _clipping = true;
    }
    inline void clearClipping(){
      _clipping = false;
      #ifdef DSP_LCD
      setClipping({0, 0, width(), height()});
      #endif
    }
  private:
    bool _clipping;
    clipArea _cliparea;
    void * _scrollid;
    #ifdef PSFBUFFER
    psFrameBuffer* _fb=nullptr;
    #endif
    #ifdef DSP_LCD
    // Sound meter state
    bool _soundMeterMode;
    uint32_t _soundMeterLastUpdate;
    uint16_t _soundMeterMeasL;
    uint16_t _soundMeterMeasR;
    uint8_t _soundMeterPeakL;
    uint8_t _soundMeterPeakR;
    uint32_t _soundMeterPeakHoldUntilL;
    uint32_t _soundMeterPeakHoldUntilR;
    uint8_t _soundMeterAutoPeak;
    char _soundMeterPrevLine[41];
    char _soundMeterPrevClockLine[41];
    bool _soundMeterVUMeterWasEnabled;
    // Spectrum analyser state
    static const uint8_t SPECTRUM_BANDS = 40; // max display width; actual used = displayWidth
    bool _spectrumMode;
    uint32_t _spectrumLastUpdate;
    uint8_t _spectrumAutoPeak;
    uint16_t _spectrumMeas[SPECTRUM_BANDS];   // smoothed level per band (0..16)
    uint8_t  _spectrumPeak[SPECTRUM_BANDS];   // peak dot per band (0..16)
    uint32_t _spectrumPeakHold[SPECTRUM_BANDS];
    char _spectrumPrevRow0[41];
    char _spectrumPrevRow1[41];
    #endif
};

extern DspCore dsp;
#endif
