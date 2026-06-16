#include "services/touch.h"

#include <Arduino.h>
#include <Wire.h>

#include <cstdlib>

#include "config.h"

namespace services::touch {

namespace {

bool s_ready = false;
bool s_in_touch = false;  // finger currently down

// Gesture tracking across a press.
int s_start_x = 0, s_start_y = 0;
int s_last_x = 0, s_last_y = 0;
int s_tap_x = 0, s_tap_y = 0;  // location of the last Tap event
unsigned long s_down_ms = 0;

constexpr int kIntPin = static_cast<int>(config::kTouchPinInt);
constexpr int kSwipeMinPx = 32;   // min horizontal travel for a swipe
constexpr int kTapMaxPx = 16;     // max travel to still count as a tap
constexpr unsigned long kTapMaxMs = 700;

volatile bool s_touch_irq = false;
void IRAM_ATTR onTouchIsr() { s_touch_irq = true; }

bool readReg(uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(config::kTouchI2cAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  const size_t got = Wire.requestFrom(static_cast<int>(config::kTouchI2cAddr),
                                      static_cast<int>(len));
  if (got != len) return false;
  for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(config::kTouchI2cAddr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

/** Read finger count + X/Y. regs 0x01..0x06: [gesture, fingers, xh,xl,yh,yl] */
bool readPoint(bool* down, int* x, int* y) {
  uint8_t b[6];
  if (!readReg(0x01, b, sizeof(b))) return false;
  *down = b[1] > 0;
  *x = ((b[2] & 0x0F) << 8) | b[3];
  *y = ((b[4] & 0x0F) << 8) | b[5];
  return true;
}

}  // namespace

void init() {
  pinMode(static_cast<uint8_t>(config::kTouchPinRst), OUTPUT);
  digitalWrite(static_cast<uint8_t>(config::kTouchPinRst), LOW);
  delay(10);
  digitalWrite(static_cast<uint8_t>(config::kTouchPinRst), HIGH);
  delay(60);

  Wire.begin(static_cast<int>(config::kTouchPinSda),
             static_cast<int>(config::kTouchPinScl), 400000);
  writeReg(0xFE, 0x01);  // disable auto-sleep so idle polling keeps ACKing

  pinMode(kIntPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(kIntPin), onTouchIsr, FALLING);
  s_ready = true;
}

Event poll() {
  if (!s_ready) return Event::None;
  s_touch_irq = false;

  bool down = false;
  int x = 0, y = 0;
  if (!readPoint(&down, &x, &y)) return Event::None;

  Event ev = Event::None;
  const unsigned long now = millis();

  if (down) {
    if (!s_in_touch) {  // press start
      s_in_touch = true;
      s_start_x = s_last_x = x;
      s_start_y = s_last_y = y;
      s_down_ms = now;
    } else {  // ongoing press — remember latest valid position
      s_last_x = x;
      s_last_y = y;
    }
  } else if (s_in_touch) {  // release — classify the gesture
    s_in_touch = false;
    const int dx = s_last_x - s_start_x;
    const int dy = s_last_y - s_start_y;
    if (std::abs(dx) >= kSwipeMinPx && std::abs(dx) > std::abs(dy)) {
      ev = dx < 0 ? Event::SwipeLeft : Event::SwipeRight;
    } else if (std::abs(dx) <= kTapMaxPx && std::abs(dy) <= kTapMaxPx &&
               now - s_down_ms <= kTapMaxMs) {
      ev = Event::Tap;
      s_tap_x = s_last_x;
      s_tap_y = s_last_y;
    }
  }
  return ev;
}

int tapX() { return s_tap_x; }
int tapY() { return s_tap_y; }

}  // namespace services::touch
