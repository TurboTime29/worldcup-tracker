#include "services/timezone.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include <ArduinoJson.h>

#include <cstdio>

#include "services/settings.h"

namespace services::timezone {

namespace {

long s_offset_sec = 0;
bool s_have = false;

}  // namespace

void init() {
  // ip-api.com is free over plain HTTP (HTTPS needs their paid tier).
  HTTPClient http;
  if (!http.begin("http://ip-api.com/json/?fields=status,offset,timezone")) {
    Serial.println("tz: http.begin failed");
    return;
  }
  http.setTimeout(8000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("tz: HTTP %d (using UTC)\n", code);
    http.end();
    return;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("tz: parse error %s\n", err.c_str());
    return;
  }
  if (doc["status"] == "success") {
    s_offset_sec = doc["offset"] | 0L;
    s_have = true;
    Serial.printf("tz: %s offset %lds\n",
                  doc["timezone"].as<const char*>() ? doc["timezone"].as<const char*>() : "?",
                  s_offset_sec);
  }
}

long offsetSeconds() { return s_offset_sec; }

void formatLocalTime(time_t utc, char* out, size_t cap) {
  time_t local = utc + s_offset_sec;
  struct tm t;
  gmtime_r(&local, &t);
  if (services::settings::clock24h()) {
    snprintf(out, cap, "%d:%02d", t.tm_hour, t.tm_min);
  } else {
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(out, cap, "%d:%02d %s", hour12, t.tm_min,
             t.tm_hour < 12 ? "AM" : "PM");
  }
  (void)s_have;
}

void formatCountdown(time_t utc, time_t now, char* out, size_t cap) {
  long secs = static_cast<long>(utc - now);
  if (secs < 0) secs = 0;
  // Round to 5-minute buckets so the screen only needs to redraw every ~5 min
  // (no per-minute flicker while a match is still hours away).
  long mins = ((secs + 150) / 300) * 5;  // nearest 5 minutes
  long h = mins / 60;
  long m = mins % 60;
  if (h <= 0 && m <= 0) {
    snprintf(out, cap, "soon");
  } else if (h <= 0) {
    snprintf(out, cap, "%ldm", m);
  } else {
    snprintf(out, cap, "%ldh %ldm", h, m);
  }
}

}  // namespace services::timezone
