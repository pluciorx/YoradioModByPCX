/*************************************************************************************
    LCD1602 displays configuration file.
    Copy this file to yoRadio/src/displays/conf/displayLCD1602conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayLCD1602conf_h
#define displayLCD1602conf_h
#if DSP_MODEL==DSP_2002 || DSP_MODEL==DSP_2002I2C
#define DSP_WIDTH       20
#elif DSP_MODEL==DSP_4002I2C
#define DSP_WIDTH       40
#else
#define DSP_WIDTH      
#endif
#define TFT_FRAMEWDT    0
#define MAX_WIDTH       DSP_WIDTH
#define PLMITEMS        2



#define HIDE_IP
#define HIDE_TITLE2
//#define HIDE_VOL
#define HIDE_VOLBAR
#define HIDE_HEAPBAR
#define HIDE_RSSI
#define HIDE_VU
#define HIDE_WEATHER
#define META_MOVE
#define VOL_PCT
//#define bootLogoTop     68

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
#define SDELTA      2
#define STIME       400
const ScrollConfig      metaConf   PROGMEM = { { 9, 0, 1, WA_CENTER }, 140, true, MAX_WIDTH - 14, 2000, SDELTA, STIME };
const ScrollConfig    title1Conf   PROGMEM = { { 0, 1, 1, WA_LEFT }, 140, false, MAX_WIDTH - 7, 2000, SDELTA, STIME };
const ScrollConfig  playlistConf   PROGMEM = { { 1, 1, 1, WA_LEFT }, 140, true, MAX_WIDTH - 1, 2000, SDELTA, STIME };

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   volbarConf     PROGMEM = { { 0, 1, 0, WA_LEFT }, MAX_WIDTH, 1, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig    bootstrConf  PROGMEM = { 0, 0, 1, WA_CENTER };
const WidgetConfig    bitrateConf  PROGMEM = { 0, 1, 1, WA_RIGHT };
const WidgetConfig        numConf  PROGMEM = { 0, 1, 1, WA_CENTER };
const WidgetConfig      clockConf  PROGMEM = { 0, 0, 1, WA_CENTER };
const WidgetConfig    bootWdtConf  PROGMEM = { 0, 1, 1, WA_CENTER };
const WidgetConfig     voltxtConf  PROGMEM = { 0, 0, 1, WA_LEFT };
const ProgressConfig  bootPrgConf  PROGMEM = { 250, 10, 4 };

/* STRINGS  */
const char         numtxtFmt[]    PROGMEM = "%d";
const char        bitrateFmt[]    PROGMEM = "%dKbs";
const char         voltxtFmt[]    PROGMEM = "Vol:%d%%";
//const char        bootstrFmt[]    PROGMEM = "Trying to %s";


/* MOVES  */                             /* { left, top, width } */
const MoveConfig   metaMove       PROGMEM = { 0, 0, MAX_WIDTH };
#endif
