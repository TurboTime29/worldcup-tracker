#include "ui/settings_screen.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <cstdio>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"

namespace ui::settings {

namespace fonts = lgfx::v1::fonts;

namespace {

constexpr int CX = 120;
constexpr uint16_t BG = config::kColorBgBottom;

constexpr auto& F_LABEL = fonts::FreeSans9pt7b;
constexpr auto& F_BTN = fonts::FreeSansBold24pt7b;
constexpr auto& F_PCT = fonts::FreeSansBold24pt7b;

// Button geometry (matched by hitTest).
constexpr int Y_ROW = 120;     // − / % / + row center
constexpr int X_MINUS = 52;
constexpr int X_PLUS = 188;
constexpr int R_BTN = 30;
constexpr int Y_DONE = 188;    // DONE pill center
constexpr int W_DONE = 96;
constexpr int H_DONE = 34;

void gfx(const char* s, int x, int y, const lgfx::GFXfont* f, uint16_t fg,
         textdatum_t datum = textdatum_t::middle_center) {
  displayFontSetBitmap(tft, f);
  tft.setTextDatum(datum);
  tft.setTextColor(fg, BG);
  tft.drawString(s, x, y);
}

}  // namespace

void draw(int percent) {
  tft.fillScreen(BG);

  gfx("BRIGHTNESS", CX, 56, &F_LABEL, config::kColorMuted);

  // − button
  tft.fillSmoothCircle(X_MINUS, Y_ROW, R_BTN, config::kColorRingTrack);
  gfx("-", X_MINUS, Y_ROW - 3, &F_BTN, config::kColorInk);

  // + button
  tft.fillSmoothCircle(X_PLUS, Y_ROW, R_BTN, config::kColorRingTrack);
  gfx("+", X_PLUS, Y_ROW - 2, &F_BTN, config::kColorInk);

  // percentage in the middle
  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", percent);
  gfx(pct, CX, Y_ROW, &F_PCT, config::kColorUp);

  // DONE pill
  tft.fillRoundRect(CX - W_DONE / 2, Y_DONE - H_DONE / 2, W_DONE, H_DONE,
                    H_DONE / 2, config::kColorWin);
  displayFontSetBitmap(tft, &F_LABEL);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(config::kColorBlack, config::kColorWin);
  tft.drawString("DONE", CX, Y_DONE - 1);
}

Hit hitTest(int x, int y) {
  // DONE first (own row near the bottom).
  if (y >= Y_DONE - H_DONE && y <= Y_DONE + H_DONE && x >= CX - W_DONE &&
      x <= CX + W_DONE) {
    return Hit::Done;
  }
  // − / + occupy the left / right of the middle row (generous finger targets).
  if (y >= Y_ROW - 55 && y <= Y_ROW + 55) {
    if (x <= X_MINUS + R_BTN + 10) return Hit::Minus;
    if (x >= X_PLUS - R_BTN - 10) return Hit::Plus;
  }
  return Hit::None;
}

}  // namespace ui::settings
