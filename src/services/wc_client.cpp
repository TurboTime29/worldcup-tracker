#include "services/wc_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "config.h"
#include "services/settings.h"
#include "services/teams.h"

namespace services::wc {

namespace {

model::Match s_matches[config::kMaxMatches];
size_t s_count = 0;
unsigned long s_last_fetch_ms = 0;    // last *successful* fetch
unsigned long s_last_attempt_ms = 0;  // last attempt (success or fail)
bool s_any_live = false;
time_t s_next_kickoff = 0;            // earliest upcoming kickoff (UTC), 0 = none

// Floor between attempts so a failing/keyless fetch can't busy-loop.
constexpr unsigned long kRetryMinMs = 15000UL;

/** Broken-down UTC time -> Unix seconds (portable; no timegm dependency). */
time_t tmUtcToUnix(int year, int mon, int day, int hh, int mm, int ss) {
  int y = year;
  int m = mon;
  y -= (m <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = static_cast<unsigned>(y - era * 400);
  unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + static_cast<unsigned>(day) - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097L + static_cast<long>(doe) - 719468L;
  return static_cast<time_t>(days * 86400L + hh * 3600L + mm * 60L + ss);
}

/** Parse "2026-06-15T16:00:00Z" -> time_t (UTC). 0 on failure. */
time_t parseIsoUtc(const char* s) {
  if (s == nullptr) return 0;
  int Y, Mo, D, H, Mi, S = 0;
  if (sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &Mo, &D, &H, &Mi, &S) < 5) return 0;
  return tmUtcToUnix(Y, Mo, D, H, Mi, S);
}

/** Map football-data.org status -> our status + paused (half-time). */
model::Status mapStatus(const char* s, bool* paused) {
  *paused = false;
  if (s == nullptr) return model::ST_UPCOMING;
  if (!strcmp(s, "IN_PLAY")) return model::ST_LIVE;
  if (!strcmp(s, "PAUSED")) {  // half-time
    *paused = true;
    return model::ST_LIVE;
  }
  if (!strcmp(s, "FINISHED") || !strcmp(s, "AWARDED")) return model::ST_FINISHED;
  return model::ST_UPCOMING;  // SCHEDULED, TIMED, SUSPENDED, POSTPONED, ...
}

/** "GROUP_F" / "LAST_16" -> "Group F" / "Last 16" (underscore->space, Title). */
void prettify(const char* src, char* out, size_t cap) {
  size_t j = 0;
  bool new_word = true;
  for (const char* p = src; *p != '\0' && j < cap - 1; ++p) {
    char c = *p;
    if (c == '_') { out[j++] = ' '; new_word = true; continue; }
    out[j++] = new_word ? static_cast<char>(toupper(c))
                        : static_cast<char>(tolower(c));
    new_word = false;
  }
  out[j] = '\0';
}

/** Stage label: prefer the group ("Group F"), else the stage ("Last 16"). */
void formatStage(JsonObjectConst item, char* out, size_t cap) {
  const char* group = item["group"].as<const char*>();
  if (group != nullptr && group[0] != '\0') {
    prettify(group, out, cap);
    return;
  }
  const char* stage = item["stage"].as<const char*>();
  if (stage == nullptr) { out[0] = '\0'; return; }
  prettify(stage, out, cap);
}

void parseTeam(JsonObjectConst t, model::Team* out) {
  const char* name = t["name"].as<const char*>();
  if (name == nullptr) name = "";
  strncpy(out->name, name, sizeof(out->name) - 1);
  out->name[sizeof(out->name) - 1] = '\0';

  const char* tla = t["tla"].as<const char*>();
  if (tla != nullptr && strlen(tla) >= 2) {
    strncpy(out->code, tla, 3);
    out->code[3] = '\0';
  } else {
    services::teams::codeForName(name, out->code);  // fallback
  }
}

void deriveWinner(model::Match* m, const char* fd_winner) {
  if (m->status != model::ST_FINISHED) { m->winner = model::WIN_NONE; return; }
  if (m->has_shootout) {
    m->winner = m->pen_home > m->pen_away ? model::WIN_HOME : model::WIN_AWAY;
    return;
  }
  if (fd_winner != nullptr) {
    if (!strcmp(fd_winner, "HOME_TEAM")) { m->winner = model::WIN_HOME; return; }
    if (!strcmp(fd_winner, "AWAY_TEAM")) { m->winner = model::WIN_AWAY; return; }
    if (!strcmp(fd_winner, "DRAW")) { m->winner = model::WIN_DRAW; return; }
  }
  if (m->home.score > m->away.score) m->winner = model::WIN_HOME;
  else if (m->away.score > m->home.score) m->winner = model::WIN_AWAY;
  else m->winner = model::WIN_DRAW;
}

/** Format a UTC instant as YYYY-MM-DD. */
void dateStr(time_t t, char out[11]) {
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(out, 11, "%Y-%m-%d", &tm);
}

/** ArduinoJson filter: pull only the fields we use. */
void buildFilter(JsonDocument& filter) {
  JsonObject f = filter["matches"].add<JsonObject>();
  f["utcDate"] = true;
  f["status"] = true;
  f["stage"] = true;
  f["group"] = true;
  f["score"]["winner"] = true;
  f["score"]["duration"] = true;
  f["score"]["fullTime"]["home"] = true;
  f["score"]["fullTime"]["away"] = true;
  f["score"]["penalties"]["home"] = true;
  f["score"]["penalties"]["away"] = true;
  f["homeTeam"]["name"] = true;
  f["homeTeam"]["tla"] = true;
  f["awayTeam"]["name"] = true;
  f["awayTeam"]["tla"] = true;
}

}  // namespace

const model::Match* matches() { return s_matches; }
size_t matchCount() { return s_count; }
bool anyLive() { return s_any_live; }
unsigned long lastFetchMs() { return s_last_fetch_ms; }

bool shouldPollNow() {
  // Floor first — so boot retries (e.g. clock not synced yet) don't busy-loop.
  if (s_last_attempt_ms != 0 && millis() - s_last_attempt_ms < kRetryMinMs) {
    return false;
  }
  if (s_last_fetch_ms == 0) return true;  // first successful fetch not done yet
  const unsigned long elapsed = millis() - s_last_fetch_ms;
  if (s_any_live) return elapsed >= config::kPollLiveMs;  // live: poll fast
  // Nothing live: stay idle (no API calls) until an upcoming kickoff arrives —
  // then poll to catch it going live. We already have the full schedule.
  if (s_next_kickoff > 0 && time(nullptr) >= s_next_kickoff) {
    return elapsed >= config::kPollKickoffCheckMs;
  }
  return false;
}

// WC 2026 runs 2026-06-11 .. 2026-07-19. The full season payload (~180 KB,
// chunked) is too big to read reliably on-device, so we pull it in 7-day
// windows and accumulate. Each window is a small, reliable request.
constexpr int kWindowDays = 7;

/** Fetch one date window and append its matches into s_matches at *n.
 *  The HTTP client/connection is owned by the caller and reused across windows
 *  (keep-alive) so we pay the TLS handshake once, not six times. */
void fetchWindow(HTTPClient& http, WiFiClientSecure& client, const char* from,
                 const char* to, size_t* n, bool* any_live) {
  String url = "https://";
  url += config::kApiHost;
  url += "/v4/competitions/";
  url += config::kCompetitionCode;
  url += "/matches?dateFrom=";
  url += from;
  url += "&dateTo=";
  url += to;

  if (!http.begin(client, url)) return;
  http.addHeader("X-Auth-Token", services::settings::apiKey());

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("wc: window %s HTTP %d\n", from, code);
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  JsonDocument filter;
  buildFilter(filter);
  JsonDocument doc;
  if (deserializeJson(doc, payload,
                      DeserializationOption::Filter(filter)) != DeserializationError::Ok) {
    Serial.printf("wc: window %s parse fail (%u bytes)\n", from,
                  static_cast<unsigned>(payload.length()));
    return;
  }
  JsonArrayConst arr = doc["matches"].as<JsonArrayConst>();
  if (arr.isNull()) return;

  for (JsonObjectConst item : arr) {
    if (*n >= config::kMaxMatches) break;
    model::Match& m = s_matches[*n];
    memset(&m, 0, sizeof(m));

    m.kickoff_utc = parseIsoUtc(item["utcDate"].as<const char*>());
    bool paused = false;
    m.status = mapStatus(item["status"].as<const char*>(), &paused);
    m.paused = paused;
    m.minute = -1;
    formatStage(item, m.stage, sizeof(m.stage));
    parseTeam(item["homeTeam"], &m.home);
    parseTeam(item["awayTeam"], &m.away);
    if (m.home.code[0] == '\0' || m.away.code[0] == '\0') continue;  // TBD slot

    JsonObjectConst score = item["score"];
    const char* duration = score["duration"].as<const char*>();
    m.has_shootout = duration != nullptr && !strcmp(duration, "PENALTY_SHOOTOUT");
    if (m.status == model::ST_UPCOMING) {
      m.home.score = -1;
      m.away.score = -1;
    } else {
      m.home.score = score["fullTime"]["home"] | 0;
      m.away.score = score["fullTime"]["away"] | 0;
    }
    if (m.has_shootout) {
      m.pen_home = score["penalties"]["home"] | 0;
      m.pen_away = score["penalties"]["away"] | 0;
    }
    deriveWinner(&m, score["winner"].as<const char*>());
    if (m.status == model::ST_LIVE) *any_live = true;
    ++(*n);
  }
}

bool fetchToday() {
  s_last_attempt_ms = millis();
  if (!services::settings::hasApiKey()) {
    Serial.println("wc: no API token set — open the setup portal to enter one");
    return false;
  }
  if (time(nullptr) < 1700000000) {  // clock not NTP-synced yet
    Serial.println("wc: clock not synced yet, skipping fetch");
    return false;
  }

  const time_t season_start = tmUtcToUnix(2026, 6, 11, 0, 0, 0);
  const time_t season_end = tmUtcToUnix(2026, 7, 19, 23, 59, 59);

  // One client/connection, reused across windows so the TLS handshake happens
  // once instead of per window.
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setReuse(true);
  http.setTimeout(config::kRequestTimeoutMs);

  size_t n = 0;
  bool any_live = false;
  for (time_t ws = season_start; ws <= season_end && n < config::kMaxMatches;
       ws += kWindowDays * 86400L) {
    char from[11], to[11];
    dateStr(ws, from);
    dateStr(ws + (kWindowDays - 1) * 86400L, to);
    fetchWindow(http, client, from, to, &n, &any_live);
  }

  if (n == 0) {
    Serial.println("wc: no matches across the tournament window");
    return false;
  }

  std::sort(s_matches, s_matches + n,
            [](const model::Match& a, const model::Match& b) {
              return a.kickoff_utc < b.kickoff_utc;
            });

  // Earliest upcoming kickoff — when to wake up and start polling again.
  s_next_kickoff = 0;
  for (size_t i = 0; i < n; ++i) {
    if (s_matches[i].status == model::ST_UPCOMING &&
        (s_next_kickoff == 0 || s_matches[i].kickoff_utc < s_next_kickoff)) {
      s_next_kickoff = s_matches[i].kickoff_utc;
    }
  }

  s_count = n;
  s_any_live = any_live;
  s_last_fetch_ms = millis();
  Serial.printf("wc: %u tournament matches (%s live)\n",
                static_cast<unsigned>(n), any_live ? "some" : "none");
  return true;
}

}  // namespace services::wc
