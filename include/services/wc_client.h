#pragma once

#include <cstddef>

#include "model.h"

// =============================================================================
// World Cup data client — API-Football (api-sports.io), direct from device.
//
// One GET /fixtures?league=1&season=2026&date=<today> returns every match for
// the day WITH live status + minute, so a single request refreshes everything.
// Budget-aware: see config::kPoll*Ms and shouldPollNow().
// =============================================================================

namespace services::wc {

/** Fetch today's matches (UTC date). Returns true on a successful refresh. */
bool fetchToday();

/** Matches from the last successful fetch (sorted by kickoff). */
const model::Match* matches();
size_t matchCount();

/** True if any match is currently live. */
bool anyLive();

/**
 * Poll gate. True at boot, then only when a match is live (fast cadence) or an
 * upcoming kickoff time has arrived; otherwise idle (no API calls).
 */
bool shouldPollNow();

/** millis() of the last successful fetch (0 if never). */
unsigned long lastFetchMs();

}  // namespace services::wc
