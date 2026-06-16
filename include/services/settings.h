#pragma once

// =============================================================================
// Persistent user settings (NVS) — entered in the WiFi setup portal.
//   - football-data.org API token
//   - favorite team (3-letter FIFA code; empty = no favorite / show live)
// =============================================================================

namespace services::settings {

/** Load saved settings into RAM. Call once early in setup(). */
void init();

/** football-data.org token, "" if unset. */
const char* apiKey();
bool hasApiKey();

/** Favorite team FIFA code, "" if none. */
const char* favoriteTeamCode();
bool hasFavorite();

/** Clock format: true = 24-hour (18:00), false = 12-hour (6:00 PM). Default 12h. */
bool clock24h();

/** Display backlight as a percentage (10..100). Default 20%. */
int brightnessPercent();

/** Persist (also updates the in-RAM copy). nullptr treated as "". */
void setApiKey(const char* token);
void setFavoriteTeamCode(const char* code);
void setClock24h(bool on);
void setBrightnessPercent(int pct);  // clamped to 10..100

}  // namespace services::settings
