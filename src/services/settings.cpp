#include "services/settings.h"

#include <Preferences.h>

#include <cstring>

#include "config.h"

namespace services::settings {

namespace {

// Separate NVS namespace from the "wifi" portal-flag prefs (wifi_setup.cpp).
constexpr char kNamespace[] = "wcprefs";
constexpr char kKeyApi[] = "apikey";
constexpr char kKeyFav[] = "favteam";
constexpr char kKeyClk[] = "clk24";
constexpr char kKeyBright[] = "bright";

constexpr int kBrightDefault = 20;
constexpr int kBrightMin = 10;
constexpr int kBrightMax = 100;

char s_api_key[config::kApiKeyMaxLen + 1] = "";
char s_fav_code[4] = "";
bool s_clock24h = false;
int s_brightness = kBrightDefault;
bool s_loaded = false;

int clampBright(int pct) {
  if (pct < kBrightMin) return kBrightMin;
  if (pct > kBrightMax) return kBrightMax;
  return pct;
}

void copyInto(char* dst, size_t cap, const char* src) {
  if (src == nullptr) src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

}  // namespace

void init() {
  Preferences prefs;
  if (prefs.begin(kNamespace, true)) {
    String api = prefs.getString(kKeyApi, "");
    String fav = prefs.getString(kKeyFav, "");
    s_clock24h = prefs.getBool(kKeyClk, false);
    s_brightness = clampBright(prefs.getInt(kKeyBright, kBrightDefault));
    prefs.end();
    copyInto(s_api_key, sizeof(s_api_key), api.c_str());
    copyInto(s_fav_code, sizeof(s_fav_code), fav.c_str());
  }
  s_loaded = true;
}

const char* apiKey() { return s_api_key; }
bool hasApiKey() { return s_api_key[0] != '\0'; }

const char* favoriteTeamCode() { return s_fav_code; }
bool hasFavorite() { return s_fav_code[0] != '\0'; }

void setApiKey(const char* token) {
  copyInto(s_api_key, sizeof(s_api_key), token);
  Preferences prefs;
  if (prefs.begin(kNamespace, false)) {
    prefs.putString(kKeyApi, s_api_key);
    prefs.end();
  }
}

void setFavoriteTeamCode(const char* code) {
  copyInto(s_fav_code, sizeof(s_fav_code), code);
  Preferences prefs;
  if (prefs.begin(kNamespace, false)) {
    prefs.putString(kKeyFav, s_fav_code);
    prefs.end();
  }
}

bool clock24h() { return s_clock24h; }
int brightnessPercent() { return s_brightness; }

void setClock24h(bool on) {
  s_clock24h = on;
  Preferences prefs;
  if (prefs.begin(kNamespace, false)) {
    prefs.putBool(kKeyClk, s_clock24h);
    prefs.end();
  }
}

void setBrightnessPercent(int pct) {
  s_brightness = clampBright(pct);
  Preferences prefs;
  if (prefs.begin(kNamespace, false)) {
    prefs.putInt(kKeyBright, s_brightness);
    prefs.end();
  }
}

}  // namespace services::settings
