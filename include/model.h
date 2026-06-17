#pragma once

#include <ctime>

// =============================================================================
// Data model — mirrors worldcup_tracker_SPEC.md §5 and the mockup GAMES array.
// =============================================================================

namespace model {

enum Status { ST_FINISHED, ST_LIVE, ST_UPCOMING };

// winner_t values for match_t::winner
enum Winner { WIN_HOME = 0, WIN_AWAY = 1, WIN_DRAW = 2, WIN_NONE = -1 };

struct Team {
  char code[4];   // 3-letter FIFA code (also flag asset key), e.g. "ESP"
  char name[20];  // display name, e.g. "Spain"
  int score;      // -1 = not played yet
};

struct Match {
  int id;              // football-data match id (stable key for in-place updates)
  char stage[20];      // e.g. "Group H", "Round of 32"
  Status status;
  time_t kickoff_utc;  // rendered in the user's local zone
  int minute;          // live only, else -1 (true value at last sync)

  Team home;
  Team away;

  bool has_shootout;
  int pen_home;
  int pen_away;
  int winner;  // see Winner enum

  // --- local minute interpolation (not from the API) ---
  // When status==ST_LIVE, minute is the API's value at last sync; we advance a
  // displayed minute locally between polls. paused==true during half-time.
  unsigned long synced_at_ms;  // millis() when `minute` was last fetched
  bool paused;                 // true at HT / break — freeze the ring
};

// Derived display minute for a live match, interpolated since last sync.
inline int liveDisplayMinute(const Match& m, unsigned long now_ms) {
  if (m.status != ST_LIVE || m.minute < 0) return m.minute;
  if (m.paused) return m.minute;
  unsigned long elapsed_min = (now_ms - m.synced_at_ms) / 60000UL;
  int v = m.minute + static_cast<int>(elapsed_min);
  return v > 130 ? 130 : v;  // clamp absurd drift
}

}  // namespace model
