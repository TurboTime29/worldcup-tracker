/**
 * World Cup Tracker — boot, setup portal, then the swipeable match UI.
 */

#include <Arduino.h>
#include <WiFi.h>

#include <cstring>
#include <ctime>

#include "config.h"
#include "hardware/display.h"
#include "model.h"
#include "services/settings.h"
#include "services/timezone.h"
#include "services/touch.h"
#include "services/wc_client.h"
#include "services/wifi_setup.h"
#include "ui/match_display.h"
#include "ui/settings_screen.h"
#include "ui/status_screens.h"

namespace {

enum class Mode { Matches, Settings };

size_t g_index = 0;
size_t g_auto_focus = 0;       // the match the view auto-follows (live/next game)
bool g_initial_jump_done = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_last_tick_ms = 0;
unsigned long g_last_focus_ms = 0;

Mode g_mode = Mode::Matches;
int g_bright = 20;             // working brightness % while the settings screen is open
char g_last_sig[80] = "";      // signature of what's on screen (skip redundant redraws)

constexpr unsigned long kRedrawTickMs = 15000;   // re-check the countdown / kickoff flip
constexpr unsigned long kFocusCheckMs = 15000;   // re-check which match to follow

/** The match the view should be on right now (matches are sorted by kickoff):
 *  a live match → the next upcoming once it's within the auto-jump lead →
 *  otherwise the most recent finished result → else the first match. */
size_t focusIndex() {
  const model::Match* ms = services::wc::matches();
  const size_t n = services::wc::matchCount();
  if (n == 0) return 0;
  const time_t now = time(nullptr);
  for (size_t i = 0; i < n; ++i)
    if (ms[i].status == model::ST_LIVE) return i;
  for (size_t i = 0; i < n; ++i)
    if (ms[i].status == model::ST_UPCOMING) {
      if (ms[i].kickoff_utc - now <= config::kAutoJumpLeadSec) return i;
      break;  // earliest upcoming is still further out than the lead
    }
  size_t last_done = n;  // sentinel
  for (size_t i = 0; i < n; ++i)
    if (ms[i].status == model::ST_FINISHED) last_done = i;
  if (last_done != n) return last_done;  // linger on the latest result
  for (size_t i = 0; i < n; ++i)
    if (ms[i].status == model::ST_UPCOMING) return i;  // not started yet
  return n - 1;
}

/** Build a short signature of what the match view would show right now, so we
 *  only repaint when something actually changed (no per-tick flicker). The
 *  countdown is bucketed to 5 minutes (matches formatCountdown). */
void currentSignature(char* sig, size_t cap) {
  const size_t n = services::wc::matchCount();
  if (n == 0) {
    if (!services::settings::hasApiKey()) snprintf(sig, cap, "m:setup");
    else if (WiFi.status() != WL_CONNECTED) snprintf(sig, cap, "m:wifi");
    else snprintf(sig, cap, "m:load");
    return;
  }
  size_t idx = g_index >= n ? n - 1 : g_index;
  const model::Match& m = services::wc::matches()[idx];
  const time_t now = time(nullptr);
  // Once kickoff passes we render an upcoming match as live (the API is delayed).
  const bool started = (m.status == model::ST_UPCOMING && now >= m.kickoff_utc);
  const int eff_status = started ? static_cast<int>(model::ST_LIVE)
                                 : static_cast<int>(m.status);
  // Track the *displayed* countdown value (rounded to 5 min, -1 when hidden) so
  // the screen repaints as it ticks down and clears it at kickoff.
  long disp = -1;
  if (m.status == model::ST_UPCOMING && !started) {
    const long secs = static_cast<long>(m.kickoff_utc - now);
    if (secs > 0 && secs <= 86400) disp = ((secs + 150) / 300) * 5;
  }
  snprintf(sig, cap, "%u/%u|%d|%d|%d|%d|%d|%ld", static_cast<unsigned>(idx),
           static_cast<unsigned>(n), eff_status, m.paused ? 1 : 0, m.home.score,
           m.away.score, static_cast<int>(m.winner), disp);
}

void renderCurrent(bool force = false) {
  g_last_tick_ms = millis();
  char sig[80];
  currentSignature(sig, sizeof(sig));
  if (!force && strcmp(sig, g_last_sig) == 0) return;  // unchanged → no flicker
  strncpy(g_last_sig, sig, sizeof(g_last_sig) - 1);
  g_last_sig[sizeof(g_last_sig) - 1] = '\0';

  const size_t n = services::wc::matchCount();
  if (n == 0) {
    if (!services::settings::hasApiKey()) {
      ui::match::drawMessage("Setup needed", "Enter API token");
    } else if (WiFi.status() != WL_CONNECTED) {
      ui::match::drawMessage("Connecting", "to Wi-Fi");
    } else {
      // matchCount()==0 with Wi-Fi + token just means we haven't completed the
      // first fetch yet (e.g. NTP still syncing) — it always loads shortly.
      ui::match::drawMessage("Loading...", "World Cup 2026");
    }
    return;
  }
  if (g_index >= n) g_index = n - 1;
  ui::match::draw(services::wc::matches()[g_index], g_index, n);
}

/** Auto-advance the view as games go live/end (e.g. jump to the next match
 *  ~15 min before kickoff). Only pulls the view along if the user is still
 *  parked on the previously-followed match, so manual browsing isn't yanked;
 *  the new focus is always tracked so a tap re-centers onto it. */
void autoFollow() {
  if (services::wc::matchCount() == 0) return;
  const size_t f = focusIndex();
  if (f == g_auto_focus) return;
  if (g_index == g_auto_focus) {
    g_index = f;
    renderCurrent();
  }
  g_auto_focus = f;
}

void refreshData(bool allow_initial_jump) {
  if (!services::wc::fetch()) {
    renderCurrent();  // surfaces "Setup needed" / "No matches"
    return;
  }
  if (allow_initial_jump && !g_initial_jump_done) {
    g_index = focusIndex();
    g_auto_focus = g_index;
    g_initial_jump_done = true;
    renderCurrent();
    return;
  }
  autoFollow();     // new statuses may change which match to follow
  renderCurrent();  // refresh scores etc. (no-op if nothing changed)
}

void enterSettings() {
  g_mode = Mode::Settings;
  g_bright = services::settings::brightnessPercent();
  ui::settings::draw(g_bright);
}

void exitSettings() {
  g_mode = Mode::Matches;
  renderCurrent(true);  // force: the settings screen overwrote the match view
}

void handleTouch() {
  const size_t n = services::wc::matchCount();
  switch (services::touch::poll()) {
    case services::touch::Event::SwipeLeft:
      if (n > 0 && g_index + 1 < n) { ++g_index; renderCurrent(); }
      break;
    case services::touch::Event::SwipeRight:
      if (n > 0 && g_index > 0) { --g_index; renderCurrent(); }
      break;
    case services::touch::Event::Tap:
      if (n > 0) { g_index = focusIndex(); g_auto_focus = g_index; renderCurrent(); }
      break;
    case services::touch::Event::None:
      break;
  }
}

/** Touch handling while the settings screen is open. */
void handleSettingsTouch() {
  if (services::touch::poll() != services::touch::Event::Tap) return;
  const int prev = g_bright;
  switch (ui::settings::hitTest(services::touch::tapX(), services::touch::tapY())) {
    case ui::settings::Hit::Minus: g_bright -= 10; break;
    case ui::settings::Hit::Plus:  g_bright += 10; break;
    case ui::settings::Hit::Done:  exitSettings(); return;
    case ui::settings::Hit::None:  return;
  }
  if (g_bright < 10) g_bright = 10;
  if (g_bright > 100) g_bright = 100;
  if (g_bright != prev) {
    displaySetBrightnessPercent(g_bright);
    services::settings::setBrightnessPercent(g_bright);
    ui::settings::draw(g_bright);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nWorld Cup Tracker");

  bootButtonInit();
  services::settings::init();
  services::touch::init();
  displayInit();

  if (wifiShowsSetupScreenOnBoot()) statusScreenPortal();

  if (wifiSetupConnect()) {
    // Show an honest "Loading" state while we sync the clock and fetch, instead
    // of leaving the connect screen frozen.
    ui::match::drawMessage("Loading...", "World Cup 2026");

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for NTP");
    // Wait until the clock is actually set (exits as soon as it syncs, usually a
    // few seconds). Doing the first fetch with a synced clock avoids the
    // post-boot retry cooldown that otherwise stretched boot to ~a minute.
    time_t now = time(nullptr);
    for (int i = 0; i < 120 && now < 1700000000; ++i) {
      delay(250);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println();

    services::timezone::init();
    refreshData(true);
  }
}

void loop() {
  bootButtonPollLongPress();
  wifiLoop();

  // A single BOOT click toggles the brightness/settings screen (a 3s hold is
  // still the Wi-Fi reset, handled by bootButtonPollLongPress above).
  if (bootButtonConsumeTap()) {
    if (g_mode == Mode::Settings) exitSettings();
    else enterSettings();
  }

  if (g_mode == Mode::Settings) {
    handleSettingsTouch();
    delay(10);
    return;
  }

  handleTouch();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_wifi_down_since == 0) g_wifi_down_since = millis();
    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        renderCurrent();
      }
    }
    delay(10);
    return;
  }

  g_wifi_down_since = 0;

  // Fetch only when due (boot, a kickoff arrived, or a match is live).
  if (services::wc::shouldPollNow()) {
    refreshData(true);
  }

  // Auto-advance the view as kickoff times approach (no fetch needed — we
  // already have the schedule, and this is time-driven).
  if (millis() - g_last_focus_ms >= kFocusCheckMs) {
    g_last_focus_ms = millis();
    autoFollow();
  }

  // Periodic redraw so an upcoming match's countdown advances between polls.
  const model::Status st = services::wc::matchCount() > 0
                               ? services::wc::matches()[g_index].status
                               : model::ST_FINISHED;
  if (st == model::ST_UPCOMING && millis() - g_last_tick_ms >= kRedrawTickMs) {
    renderCurrent();
  }

  delay(10);
}
