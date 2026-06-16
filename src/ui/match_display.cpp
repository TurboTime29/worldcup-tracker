#include "ui/match_display.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "assets/flags.h"
#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/timezone.h"

namespace ui::match {

namespace fonts = lgfx::v1::fonts;

namespace {

constexpr int CX = 120;
constexpr int CY = 120;
constexpr uint16_t BG = config::kColorBgBottom;

// Native bitmap fonts — crisp and a known size (no fractional upscaling).
constexpr auto& F_LABEL = fonts::FreeSans9pt7b;       // ~12px: stage, status, footer
constexpr auto& F_CODE = fonts::FreeSansBold12pt7b;   // ~16px: codes, dash, kickoff
constexpr auto& F_SCORE = fonts::FreeSansBold18pt7b;  // ~24px: score

constexpr int Y_STAGE = 53;
constexpr int Y_STATUS = 75;
constexpr int Y_FLAG = 105;  // flag center
constexpr int Y_CODE = 134;  // code center
constexpr int Y_SCORE = 166; // score / kickoff center
constexpr int Y_FOOTER = 192;
constexpr int Y_DOTS = 209;

constexpr int X_HOME = 70;
constexpr int X_AWAY = 170;

void gfx(const char* s, int x, int y, const lgfx::GFXfont* f, uint16_t fg,
         textdatum_t datum = textdatum_t::middle_center) {
  displayFontSetBitmap(tft, f);
  tft.setTextDatum(datum);
  tft.setTextColor(fg, BG);
  tft.drawString(s, x, y);
}

void upper(const char* in, char* out, size_t cap) {
  size_t i = 0;
  for (; in[i] != '\0' && i < cap - 1; ++i) {
    out[i] = static_cast<char>(toupper(static_cast<unsigned char>(in[i])));
  }
  out[i] = '\0';
}

// Draw a flag via run-length fillRect so each pixel goes through the SAME color
// path as text (which renders correctly) — pushImage(uint16_t*) bypassed it.
void drawFlag(const char* code, int cx, int yc) {
  uint16_t w = 0, h = 0;
  const uint16_t* img = assets::flagImage(code, &w, &h);
  if (img == nullptr) return;
  const int x0 = cx - w / 2;
  const int y0 = yc - h / 2;
  tft.startWrite();
  for (uint16_t py = 0; py < h; ++py) {
    uint16_t px = 0;
    while (px < w) {
      const uint16_t c = img[py * w + px];
      uint16_t run = 1;
      while (px + run < w && img[py * w + px + run] == c) ++run;
      tft.writeFillRect(x0 + px, y0 + py, run, 1, c);
      px += run;
    }
  }
  tft.endWrite();
}

void drawStatus(const model::Match& m) {
  if (m.status == model::ST_LIVE) {
    const char* label = m.paused ? "HALF TIME" : "LIVE";
    displayFontSetBitmap(tft, &F_LABEL);
    const int tw = tft.textWidth(label);
    const int left = CX - (tw + 12) / 2;
    tft.fillSmoothCircle(left + 4, Y_STATUS, 3, config::kColorLive);
    gfx(label, left + 12, Y_STATUS, &F_LABEL, config::kColorLive,
        textdatum_t::middle_left);
  } else if (m.status == model::ST_UPCOMING) {
    // "TODAY" if the match is on the user's local day, else the date (MM/DD/YY).
    const long off = services::timezone::offsetSeconds();
    const time_t kl = m.kickoff_utc + off;
    const time_t nl = time(nullptr) + off;
    struct tm kt, nt;
    gmtime_r(&kl, &kt);
    gmtime_r(&nl, &nt);
    static const char* kMonths[] = {"January", "February", "March", "April",
                                    "May", "June", "July", "August", "September",
                                    "October", "November", "December"};
    char lbl[24];
    if (kt.tm_year == nt.tm_year && kt.tm_yday == nt.tm_yday) {
      strcpy(lbl, "TODAY");
    } else {
      snprintf(lbl, sizeof(lbl), "%s %d, %d", kMonths[kt.tm_mon], kt.tm_mday,
               kt.tm_year + 1900);
    }
    gfx(lbl, CX, Y_STATUS, &F_LABEL, config::kColorUp);
  } else {
    gfx("FULL TIME", CX, Y_STATUS, &F_LABEL, config::kColorMuted);
  }
}

void drawTeam(const model::Team& t, int x, int winner_state) {
  uint16_t code_color = config::kColorInk;
  if (winner_state == 1) code_color = config::kColorWin;
  else if (winner_state == -1) code_color = config::kColorMuted;

  drawFlag(t.code, x, Y_FLAG);  // t.code is the API tla — flags keyed the same
  gfx(t.code, x, Y_CODE, &F_CODE, code_color);
  if (winner_state == 1) {
    tft.fillRoundRect(x - 12, Y_CODE + 13, 24, 3, 1, config::kColorWin);
  }
}

void drawScore(const model::Match& m) {
  uint16_t hc = config::kColorInk, ac = config::kColorInk;
  if (m.status == model::ST_FINISHED) {
    if (m.winner == model::WIN_HOME) { hc = config::kColorWin; ac = config::kColorMuted; }
    else if (m.winner == model::WIN_AWAY) { ac = config::kColorWin; hc = config::kColorMuted; }
  }
  char h[6], a[6];
  snprintf(h, sizeof(h), "%d", m.home.score < 0 ? 0 : m.home.score);
  snprintf(a, sizeof(a), "%d", m.away.score < 0 ? 0 : m.away.score);
  gfx("-", CX, Y_SCORE, &F_CODE, config::kColorMuted);
  gfx(h, CX - 16, Y_SCORE, &F_SCORE, hc, textdatum_t::middle_right);
  gfx(a, CX + 16, Y_SCORE, &F_SCORE, ac, textdatum_t::middle_left);
}

void drawUpcomingCenter(const model::Match& m) {
  char ko[16];
  services::timezone::formatLocalTime(m.kickoff_utc, ko, sizeof(ko));
  gfx(ko, CX, Y_SCORE, &F_CODE, config::kColorInk);
}

void drawFooter(const model::Match& m) {
  char buf[28];
  if (m.status == model::ST_UPCOMING) {
    // Only show a countdown when kickoff is within 24h; keep it short.
    const time_t now = time(nullptr);
    const long secs = static_cast<long>(m.kickoff_utc - now);
    if (secs > 0 && secs <= 86400) {
      char cd[14];
      services::timezone::formatCountdown(m.kickoff_utc, now, cd, sizeof(cd));
      snprintf(buf, sizeof(buf), "IN %s", cd);
      char up_buf[20];
      upper(buf, up_buf, sizeof(up_buf));
      gfx(up_buf, CX, Y_FOOTER, &F_LABEL, config::kColorUp);
    }
    return;
  }
  if (m.status == model::ST_FINISHED) {
    if (m.has_shootout) {
      const char* wc = (m.winner == model::WIN_HOME) ? m.home.code : m.away.code;
      int ph = m.pen_home, pa = m.pen_away;
      snprintf(buf, sizeof(buf), "%s WIN %d-%d PENS", wc, ph > pa ? ph : pa,
               ph > pa ? pa : ph);
      gfx(buf, CX, Y_FOOTER, &F_LABEL, config::kColorWin);
    } else if (m.winner == model::WIN_DRAW) {
      gfx("DRAW", CX, Y_FOOTER, &F_LABEL, config::kColorMuted);
    }
  }
}

// Carousel-style page dots: the focused match is always the center dot, with up
// to kSide dots fanning left (earlier matches) and right (later matches). The
// window slides as you swipe — it does not show one dot per match. The outermost
// dot shrinks when there are still more matches beyond the visible window.
void drawDots(size_t index, size_t total) {
  if (total <= 1) return;
  constexpr int kSide = 4;   // max dots on each side of the centered one
  constexpr int gap = 12;
  const int idx = static_cast<int>(index);
  const int last = static_cast<int>(total) - 1;

  const int left = idx < kSide ? idx : kSide;
  const int right = (last - idx) < kSide ? (last - idx) : kSide;
  const bool more_left = idx > kSide;            // earlier matches off-window
  const bool more_right = (last - idx) > kSide;  // later matches off-window

  for (int d = -left; d <= right; ++d) {
    const int x = CX + d * gap;
    if (d == 0) {
      tft.fillRoundRect(x - 5, Y_DOTS - 2, 11, 4, 2, config::kColorInk);  // active
    } else {
      const bool edge = (d == -left && more_left) || (d == right && more_right);
      tft.fillSmoothCircle(x, Y_DOTS, edge ? 1 : 2, 0x4208);
    }
  }
}

}  // namespace

void draw(const model::Match& m, size_t index, size_t total) {
  tft.fillScreen(BG);

  char stage_up[24];
  upper(m.stage, stage_up, sizeof(stage_up));
  gfx(stage_up, CX, Y_STAGE, &F_LABEL, config::kColorMuted);

  drawStatus(m);

  int hs = 0, as = 0;
  if (m.status == model::ST_FINISHED) {
    if (m.winner == model::WIN_HOME) { hs = 1; as = -1; }
    else if (m.winner == model::WIN_AWAY) { hs = -1; as = 1; }
  }
  drawTeam(m.home, X_HOME, hs);
  drawTeam(m.away, X_AWAY, as);

  if (m.status == model::ST_UPCOMING) drawUpcomingCenter(m);
  else drawScore(m);

  drawFooter(m);
  drawDots(index, total);
}

void drawMessage(const char* line1, const char* line2) {
  tft.fillScreen(BG);
  gfx(line1, CX, CY - (line2 && line2[0] ? 12 : 0), &F_CODE, config::kColorInk);
  if (line2 && line2[0]) gfx(line2, CX, CY + 14, &F_LABEL, config::kColorMuted);
}

}  // namespace ui::match
