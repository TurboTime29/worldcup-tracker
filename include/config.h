#pragma once

#include <cstddef>
#include <cstdint>

#include <driver/gpio.h>

// =============================================================================
// World Cup Tracker — config
//
// Hardware layer (display/touch/boot pins) is identical to the FlightTracker
// project: same Waveshare ESP32-S3-Touch-LCD-1.28 board, pins already verified.
// Only the app layer (API, poll cadences, colors) differs.
// =============================================================================

namespace config {

// --- Wi-Fi portal (first-boot captive portal, saved to NVS) ---
constexpr char kPortalApName[] = "WorldCup-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local"); browser: http://worldcup.local */
constexpr char kPortalHostname[] = "worldcup";
constexpr char kPortalHostUrl[] = "worldcup.local";

constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
constexpr unsigned long kWifiDownGraceMs = 4000;
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

// --- BOOT button (active LOW, hardware pull-up) ---
// GPIO9 is display CS on this board, so the user button is BOOT on GPIO0.
constexpr gpio_num_t kBootPin = GPIO_NUM_0;
constexpr unsigned long kBootResetHoldMs = 3000UL;
constexpr unsigned long kBootTapMinMs = 40UL;

// --- Display: GC9A01 1.28" round 240x240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_14;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_9;
constexpr gpio_num_t kDisplayPinDc = GPIO_NUM_8;
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_11;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_10;  // display SCL
constexpr gpio_num_t kDisplayPinBacklight = GPIO_NUM_2;  // PWM backlight (active HIGH)

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;
constexpr int kDisplayOffsetX = 0;
constexpr int kDisplayOffsetY = 0;

constexpr uint32_t kDisplaySpiWriteHz = 40000000;
constexpr bool kDisplayInvert = true;
// false = BGR. This panel swaps red/blue otherwise (Japan's red disc showed
// blue, Netherlands red/blue reversed, "LIVE"/ring looked blue).
constexpr bool kDisplayRgbOrder = false;

// --- Touchscreen: CST816 (I2C) ---
constexpr gpio_num_t kTouchPinSda = GPIO_NUM_6;
constexpr gpio_num_t kTouchPinScl = GPIO_NUM_7;
constexpr gpio_num_t kTouchPinInt = GPIO_NUM_5;
constexpr gpio_num_t kTouchPinRst = GPIO_NUM_13;
constexpr uint8_t kTouchI2cAddr = 0x15;

// =============================================================================
// --- football-data.org (free tier) ---
// Register free at https://www.football-data.org/client/register for a token.
// Free plan: 10 calls/min, World Cup ("WC") included, NO hard daily cap.
// Endpoint: GET /v4/competitions/WC/matches?dateFrom=<d>&dateTo=<d>
//           header: X-Auth-Token: <token>
// Each team carries a 3-letter `tla` (e.g. "ESP"), so codes come from the API.
// Caveat: no in-play minute and scores are server-delayed a few minutes, so
// the time-ring is approximated from kickoff (frozen at half-time / PAUSED).
//
// The token and favorite team are entered on-device in the WiFi setup portal
// and stored in NVS (services/settings.*) — no key is compiled into firmware.
//
// VERIFY your token once with curl before pasting it into the portal:
//   curl -s "https://api.football-data.org/v4/competitions/WC/matches?dateFrom=2026-06-15&dateTo=2026-06-15" \
//        -H "X-Auth-Token: YOUR_TOKEN"
// =============================================================================
constexpr char kApiHost[] = "api.football-data.org";
constexpr char kCompetitionCode[] = "WC";  // FIFA World Cup
constexpr int kMaxMatches = 104;           // whole-tournament fixture list (WC 2026 = 104)
constexpr size_t kApiKeyMaxLen = 80;       // football-data tokens are ~40 chars

// --- Poll cadence ---
// To minimize API calls: fetch once at boot, then DON'T poll while nothing is
// live — we already have the schedule, so we only start polling again once an
// upcoming match's kickoff time has arrived. While a match is live, poll fast.
constexpr unsigned long kPollLiveMs = 120000UL;          // 2 min: a match is live
constexpr unsigned long kPollKickoffCheckMs = 120000UL;  // 2 min: a kickoff time has passed
constexpr unsigned long kRequestTimeoutMs = 15000UL;

// Between polls the (kickoff-approximated) live minute advances locally so the
// ring ticks smoothly; it re-bases off kickoff on every successful poll.

// =============================================================================
// --- UI colors (RGB565) — from worldcup_tracker_mockup.html ---
// =============================================================================
constexpr uint16_t kColorInk = 0xEF9F;     // #eef3f8 text
constexpr uint16_t kColorMuted = 0x7411;   // #76808f labels / dim
constexpr uint16_t kColorLive = 0xFA09;    // #ff414d live red
constexpr uint16_t kColorWin = 0x36AF;     // #37d67a winner green
constexpr uint16_t kColorUp = 0xFDA9;      // #ffb648 upcoming amber
constexpr uint16_t kColorBgTop = 0x1105;   // #16202e screen bg top
constexpr uint16_t kColorBgBottom = 0x0862; // #0a0e15 screen bg bottom
constexpr uint16_t kColorRingTrack = 0x18E3; // faint ring track
constexpr uint16_t kColorBlack = 0x0000;

// Status / setup screens (ported from FlightTracker — yellow setup screens).
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
