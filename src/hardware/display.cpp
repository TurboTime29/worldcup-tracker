#include "hardware/display.h"

#include "hardware/display_font.h"
#include "services/settings.h"

LGFX tft;

void displaySetBrightnessPercent(int pct) {
  if (pct < 10) pct = 10;
  if (pct > 100) pct = 100;
  // Map 10..100% onto a usable backlight range (min ~13/255 so it never blacks
  // out). 255 is full; the panel is comfortably readable from ~13 up.
  const int duty = 13 + (pct - 10) * (255 - 13) / 90;
  tft.setBrightness(static_cast<uint8_t>(duty));
}

void displayInit() {
  tft.init();
  tft.setRotation(0);
  tft.setTextWrap(false);
  displaySetBrightnessPercent(services::settings::brightnessPercent());
  displayFontInit();
}
