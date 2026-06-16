#include "services/wifi_setup.h"

#include <DNSServer.h>
#include <WiFi.h>
#include <WebServer.h>

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif

#include <Preferences.h>
#include <esp_system.h>

#include <cstring>

#include "config.h"
#include "services/settings.h"
#include "ui/status_screens.h"

// =============================================================================
// Custom captive portal (DNSServer + WebServer + Preferences).
// One page: API token -> Wi-Fi -> password.
// Save persists everything to NVS and reboots; the device then connects.
// Pattern per common ESP32 provisioning examples (RNT / WiFiConfigManager).
// =============================================================================

// ---- BOOT button (verbatim from FlightTracker) ----
portMUX_TYPE s_boot_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool s_boot_tap_pending = false;
volatile bool s_boot_is_down = false;
volatile unsigned long s_boot_down_ms = 0;
bool s_long_press_handled = false;
bool s_boot_interrupt_attached = false;

void IRAM_ATTR onBootButtonIsr() {
  const bool down = digitalRead(config::kBootPin) == LOW;
  const unsigned long now = millis();
  portENTER_CRITICAL_ISR(&s_boot_mux);
  if (down) {
    s_boot_is_down = true;
    s_boot_down_ms = now;
  } else if (s_boot_is_down) {
    const unsigned long held = now - s_boot_down_ms;
    if (held >= config::kBootTapMinMs && held < config::kBootResetHoldMs) {
      s_boot_tap_pending = true;
    }
    s_boot_is_down = false;
  }
  portEXIT_CRITICAL_ISR(&s_boot_mux);
}

void initBootButton() {
  pinMode(config::kBootPin, INPUT_PULLUP);
  if (s_boot_interrupt_attached) {
    return;
  }
  attachInterrupt(digitalPinToInterrupt(static_cast<uint8_t>(config::kBootPin)),
                  onBootButtonIsr, CHANGE);
  s_boot_interrupt_attached = true;
}

namespace {

constexpr char kWifiNs[] = "wifi";
constexpr char kKeySsid[] = "ssid";
constexpr char kKeyPass[] = "pass";
constexpr char kKeyPortal[] = "portal";

const IPAddress kApIp(192, 168, 4, 1);

WebServer s_server(80);
DNSServer s_dns;
bool s_routes_set = false;
bool s_server_up = false;
bool s_ap_active = false;

bool s_reboot_pending = false;
unsigned long s_reboot_at = 0;

// ---- NVS helpers ----
String nvsGet(const char* key) {
  Preferences p;
  String v;
  if (p.begin(kWifiNs, true)) {
    v = p.getString(key, "");
    p.end();
  }
  return v;
}

void nvsSetStr(const char* key, const char* val) {
  Preferences p;
  if (p.begin(kWifiNs, false)) {
    p.putString(key, val ? val : "");
    p.end();
  }
}

void nvsSetBool(const char* key, bool val) {
  Preferences p;
  if (p.begin(kWifiNs, false)) {
    p.putBool(key, val);
    p.end();
  }
}

bool nvsGetBool(const char* key) {
  Preferences p;
  bool v = false;
  if (p.begin(kWifiNs, true)) {
    v = p.getBool(key, false);
    p.end();
  }
  return v;
}

bool wifiLinkUp() {
  return WiFi.status() == WL_CONNECTED &&
         WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

bool haveSavedWifi() { return nvsGet(kKeySsid).length() > 0; }

// ---- HTML ----
void htmlEscape(const String& in, String& out) {
  for (char c : in) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
}

String buildConfigPage() {
  const String saved_ssid = nvsGet(kKeySsid);
  String api_esc;
  htmlEscape(services::settings::apiKey(), api_esc);

  // Scan networks (AP_STA / STA both allow this).
  int n = WiFi.scanNetworks();

  String p;
  p.reserve(4096);
  p += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>World Cup Tracker setup</title><style>"
         "*{box-sizing:border-box}body{margin:0;padding:18px;background:#0d1117;"
         "color:#e6edf3;font-family:-apple-system,Segoe UI,Roboto,sans-serif}"
         ".card{max-width:430px;margin:0 auto;background:#161b22;border:1px solid #2b333c;"
         "border-radius:14px;padding:20px}"
         "h1{font-size:19px;margin:0 0 2px}p.sub{color:#8b949e;font-size:13px;margin:0 0 16px}"
         "label{display:block;font-size:13px;font-weight:600;margin:14px 0 5px}"
         "input,select{width:100%;padding:11px;border-radius:9px;border:1px solid #30363d;"
         "background:#0d1117;color:#e6edf3;font-size:15px}"
         "hr{border:0;border-top:1px solid #2b333c;margin:20px 0}"
         "button{width:100%;margin-top:20px;padding:13px;border:0;border-radius:9px;"
         "background:#2ea043;color:#fff;font-size:16px;font-weight:700}"
         "small{color:#8b949e;font-size:12px}a{color:#58a6ff}"
         "</style></head><body><div class='card'>"
         "<h1>&#9917; World Cup Tracker</h1>"
         "<p class='sub'>Enter your data token, then your Wi-Fi.</p>"
         "<form action='/save' method='POST'>");

  // 1) API token
  p += F("<label>football-data.org API token</label>"
         "<input name='apikey' value='");
  p += api_esc;
  p += F("' placeholder='paste your token' autocomplete='off'>"
         "<small>Free token at football-data.org/client/register</small>");

  p += F("<hr>");

  // 2) Wi-Fi network
  p += F("<label>Wi-Fi network</label><select name='ssid'>");
  bool saw_saved = false;
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    String esc;
    htmlEscape(ssid, esc);
    p += "<option value='";
    p += esc;
    p += '\'';
    if (ssid == saved_ssid) { p += " selected"; saw_saved = true; }
    p += '>';
    p += esc;
    if (WiFi.RSSI(i) >= -60) p += " &#9679;";  // strong-signal dot
    p += "</option>";
  }
  if (saved_ssid.length() > 0 && !saw_saved) {
    String esc;
    htmlEscape(saved_ssid, esc);
    p += "<option value='" + esc + "' selected>" + esc + " (saved)</option>";
  }
  if (n <= 0 && saved_ssid.length() == 0) {
    p += F("<option value=''>(no networks found &mdash; rescan)</option>");
  }
  p += F("</select>");

  // 4) Password
  p += F("<label>Wi-Fi password</label>"
         "<input name='pass' type='password' placeholder='leave blank if open'>");

  // 5) Clock format
  p += F("<hr><label>Clock format</label><select name='clock'>");
  if (services::settings::clock24h()) {
    p += F("<option value='12'>12-hour (6:00 PM)</option>"
           "<option value='24' selected>24-hour (18:00)</option>");
  } else {
    p += F("<option value='12' selected>12-hour (6:00 PM)</option>"
           "<option value='24'>24-hour (18:00)</option>");
  }
  p += F("</select>");

  p += F("<button type='submit'>Save &amp; Connect</button>"
         "</form></div></body></html>");

  WiFi.scanDelete();
  return p;
}

String buildSavedPage(const String& ssid) {
  String esc;
  htmlEscape(ssid, esc);
  String p;
  p.reserve(800);
  p += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>Saved</title><style>body{margin:0;padding:30px;background:#0d1117;"
         "color:#e6edf3;font-family:-apple-system,Segoe UI,Roboto,sans-serif;text-align:center}"
         ".card{max-width:380px;margin:40px auto;background:#161b22;border:1px solid #2b333c;"
         "border-radius:14px;padding:26px}h1{font-size:20px}p{color:#8b949e}"
         "</style></head><body><div class='card'><h1>&#10003; Saved</h1>"
         "<p>Restarting and connecting to<br><b style='color:#e6edf3'>");
  p += esc;
  p += F("</b>&hellip;</p><p>You can close this page. If the screen shows an "
         "error, hold BOOT 3s to reopen setup.</p></div></body></html>");
  return p;
}

// ---- Handlers ----
void handleRoot() { s_server.send(200, "text/html", buildConfigPage()); }

void handleSave() {
  const String ssid = s_server.arg("ssid");
  const String pass = s_server.arg("pass");
  const String apikey = s_server.arg("apikey");
  const String clock = s_server.arg("clock");

  services::settings::setApiKey(apikey.c_str());
  services::settings::setClock24h(clock == "24");
  if (ssid.length() > 0) {
    nvsSetStr(kKeySsid, ssid.c_str());
    nvsSetStr(kKeyPass, pass.c_str());
  }
  nvsSetBool(kKeyPortal, false);  // clear any force-portal flag

  Serial.printf("Portal saved: ssid='%s' token=%s\n", ssid.c_str(),
                apikey.length() ? "set" : "empty");

  s_server.send(200, "text/html", buildSavedPage(ssid));
  s_reboot_pending = true;
  s_reboot_at = millis();
}

/** Captive-portal: send everything else to the config page. */
void handleNotFound() {
  s_server.sendHeader("Location", String("http://") + kApIp.toString() + "/",
                      true);
  s_server.send(302, "text/plain", "");
}

void ensureRoutes() {
  if (s_routes_set) return;
  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/save", HTTP_POST, handleSave);
  s_server.onNotFound(handleNotFound);
  s_routes_set = true;
}

void serviceRebootIfPending() {
  if (s_reboot_pending && millis() - s_reboot_at > 1200) {
    Serial.println("Rebooting to apply settings...");
    delay(150);
    esp_restart();
  }
}

void stopServer() {
  if (!s_server_up) return;
  s_server.stop();
#ifdef WM_MDNS
  MDNS.end();
#endif
  if (s_ap_active) {
    s_dns.stop();
    WiFi.softAPdisconnect(true);
    s_ap_active = false;
  }
  s_server_up = false;
}

void startStaConfigServer() {
  if (s_server_up) return;
  ensureRoutes();
  s_server.begin();
  s_server_up = true;
  s_ap_active = false;
#ifdef WM_MDNS
  MDNS.end();
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
#endif
  Serial.printf("LAN setup page: http://%s.local or http://%s\n",
                config::kPortalHostname, WiFi.localIP().toString().c_str());
}

/** Blocking AP captive portal — returns only via reboot on save. */
void runApPortal() {
  stopServer();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
  WiFi.softAP(config::kPortalApName);
  delay(200);

  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", kApIp);

  ensureRoutes();
  s_server.begin();
  s_server_up = true;
  s_ap_active = true;

#ifdef WM_MDNS
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
#endif

  statusScreenPortal();
  Serial.printf("Setup portal AP '%s' -> http://%s\n", config::kPortalApName,
                kApIp.toString().c_str());

  for (;;) {
    s_dns.processNextRequest();
    s_server.handleClient();
    bootButtonPollLongPress();
    serviceRebootIfPending();
    delay(2);
  }
}

bool connectSaved(bool show_ui) {
  const String ssid = nvsGet(kKeySsid);
  if (ssid.length() == 0) return false;
  const String pass = nvsGet(kKeyPass);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  if (show_ui) statusScreenConnectingBegin(ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  const unsigned long deadline =
      millis() + config::kWifiConnectAttemptMs * config::kWifiConnectAttempts;
  while (millis() < deadline) {
    if (wifiLinkUp()) return true;
    bootButtonPollLongPress();
    if (show_ui) statusScreenConnectingTick();
    delay(config::kWifiConnectingFrameMs);
  }
  return wifiLinkUp();
}

}  // namespace

// ---- Public API (unchanged signatures; used by main.cpp) ----

bool wifiShowsSetupScreenOnBoot() { return nvsGetBool(kKeyPortal); }

bool wifiBootButtonPressed() { return digitalRead(config::kBootPin) == LOW; }

void bootButtonInit() { initBootButton(); }

bool bootButtonConsumeTap() {
  portENTER_CRITICAL(&s_boot_mux);
  const bool tap = s_boot_tap_pending;
  if (tap) s_boot_tap_pending = false;
  portEXIT_CRITICAL(&s_boot_mux);
  return tap;
}

void bootButtonPollLongPress() {
  if (wifiBootButtonPressed()) {
    portENTER_CRITICAL(&s_boot_mux);
    if (!s_boot_is_down) {
      s_boot_is_down = true;
      s_boot_down_ms = millis();
    }
    const unsigned long down_ms = s_boot_down_ms;
    portEXIT_CRITICAL(&s_boot_mux);

    if (!s_long_press_handled &&
        millis() - down_ms >= config::kBootResetHoldMs) {
      s_long_press_handled = true;
      Serial.println("BOOT held — resetting WiFi");
      wifiResetCredentialsAndReboot();
    }
  } else {
    portENTER_CRITICAL(&s_boot_mux);
    s_boot_is_down = false;
    portEXIT_CRITICAL(&s_boot_mux);
    s_long_press_handled = false;
  }
}

void wifiResetCredentialsAndReboot() {
  // Wipe Wi-Fi creds, force portal next boot. Keep token + favorite team.
  nvsSetStr(kKeySsid, "");
  nvsSetStr(kKeyPass, "");
  nvsSetBool(kKeyPortal, true);
  Serial.println("WiFi credentials cleared (token/team kept)");
  statusScreenWifiReset();
  delay(800);
  esp_restart();
}

bool wifiReconnect() {
  initBootButton();
  Serial.println("WiFi reconnecting...");
  return connectSaved(true);
}

void wifiLoop() {
  if (wifiLinkUp()) {
    startStaConfigServer();
    if (s_server_up) s_server.handleClient();
    serviceRebootIfPending();
  } else {
    if (s_server_up && !s_ap_active) stopServer();
  }
}

bool wifiSetupConnect() {
  initBootButton();

  const bool force_portal = nvsGetBool(kKeyPortal);
  if (force_portal) {
    nvsSetBool(kKeyPortal, false);
    Serial.println("Opening WiFi setup portal (forced)");
    runApPortal();  // blocks until save+reboot
    return false;   // unreachable
  }

  if (haveSavedWifi() && connectSaved(true)) {
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  if (haveSavedWifi()) {
    Serial.println("Saved WiFi could not connect — opening setup portal");
  } else {
    Serial.println("No saved WiFi — opening setup portal");
  }
  runApPortal();  // blocks until save+reboot
  return false;   // unreachable
}
