#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 40x2 (4002) I2C LCD test sketch
// Typical addresses: 0x27 or 0x3F
// Typical ESP32 I2C pins: SDA=21, SCL=22

#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN SDA
#endif

#ifndef I2C_SCL_PIN 
#define I2C_SCL_PIN SCL
#endif

#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR 0x27
#endif

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 40, 2);

// ---------------- ENCODER CONFIG ----------------
#ifndef ENC_PIN_A
#define ENC_PIN_A 4 // S1
#endif
#ifndef ENC_PIN_B
#define ENC_PIN_B 3 // S2
#endif
#ifndef ENC_PIN_SW
#define ENC_PIN_SW 14 // SW (pushbutton)
#endif

volatile int32_t encoderPos = 0;
volatile uint32_t encoderLastTs = 0;
volatile bool encoderMoved = false;

// Basic ISR: read both pins and increment/decrement position
void IRAM_ATTR encoderISR() {
  uint8_t a = digitalRead(ENC_PIN_A);
  uint8_t b = digitalRead(ENC_PIN_B);
  // simple direction detection: when A changes, check B
  if (a == b) encoderPos++; else encoderPos--;
  encoderLastTs = millis();
  encoderMoved = true;
}

// Button handling (debounced, long press)
unsigned long btnLastDebounce = 0;
int btnLastState = HIGH;
int btnStableState = HIGH;
bool btnLongHandled = false;
const unsigned long BTN_DEBOUNCE = 50;
const unsigned long BTN_LONGPRESS = 1000;

void handleButton() {
  int reading = digitalRead(ENC_PIN_SW);
  unsigned long now = millis();
  if (reading != btnLastState) {
    btnLastDebounce = now;
  }
  if ((now - btnLastDebounce) > BTN_DEBOUNCE) {
    if (reading != btnStableState) {
      btnStableState = reading;
      if (btnStableState == LOW) {
        // pressed
        btnLongHandled = false;
      } else {
        // released: short click if long press wasn't handled
        if (!btnLongHandled) {
          // short click action
          // example: reset encoder position
          encoderPos = 0;
        }
      }
    }
  }
  // long press detection
  if (btnStableState == LOW && !btnLongHandled && (now - btnLastDebounce) > BTN_LONGPRESS) {
    // long press action
    encoderPos = 0; // example: also reset (user can customize)
    btnLongHandled = true;
  }
  btnLastState = reading;
}

uint8_t findLcdAddress() {
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      return addr;
    }
  }
  return 0;
}

void printCentered(uint8_t row, const char* text) {
  size_t len = strlen(text);
  uint8_t col = 0;
  if (len < 40) col = (40 - len) / 2;
  lcd.setCursor(0, row);
  lcd.print("                                        ");
  lcd.setCursor(col, row);
  lcd.print(text);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.printf("I2C init SDA=%d SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

  uint8_t found = findLcdAddress();
  if (found) {
    Serial.printf("I2C device found at 0x%02X\n", found);
  } else {
    Serial.println("No I2C device found");
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // encoder pins
  pinMode(ENC_PIN_A, INPUT_PULLUP);
  pinMode(ENC_PIN_B, INPUT_PULLUP);
  pinMode(ENC_PIN_SW, INPUT_PULLUP);
  // attach ISR to one channel (CHANGE) for simple decoding
  if(digitalPinToInterrupt(ENC_PIN_A) != NOT_AN_INTERRUPT) {
    attachInterrupt(digitalPinToInterrupt(ENC_PIN_A), encoderISR, CHANGE);
  } else {
    Serial.println("Warning: ENC_PIN_A cannot be used as interrupt pin");
  }

  printCentered(0, "yoRadio 4002 LCD TEST");
  printCentered(1, "Init OK");
  delay(1500);
}

void loop() {
  static uint32_t sec = 0;
  char line0[41];
  char line1[41];

  // poll button logic
  handleButton();

  // copy encoder atomically
  noInterrupts();
  int32_t pos = encoderPos;
  bool moved = encoderMoved;
  encoderMoved = false;
  interrupts();

  const char* bstate = (btnStableState == LOW) ? "PRESSED" : "RELEASE";

  // build lines
  snprintf(line0, sizeof(line0), "Enc: %ld %s", (long)pos, moved?"*":" ");
  snprintf(line1, sizeof(line1), "Btn:%s  Up:%lus Heap:%lu", bstate, (unsigned long)sec, (unsigned long)ESP.getFreeHeap());

  // pad/clear and print
  lcd.setCursor(0, 0);
  lcd.print("                                        ");
  lcd.setCursor(0, 0);
  lcd.print(line0);

  lcd.setCursor(0, 1);
  lcd.print("                                        ");
  lcd.setCursor(0, 1);
  lcd.print(line1);

  // also print to Serial so user can monitor
  Serial.printf("%s | %s\n", line0, line1);

  sec++;
  delay(200);
}
