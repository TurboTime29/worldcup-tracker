#pragma once

#include <cstddef>
#include <ctime>

// =============================================================================
// Local timezone via IP geolocation (free, no key): ip-api.com returns the
// current UTC offset in seconds (DST included). We store that offset and apply
// it when formatting kickoff times — no POSIX TZ database needed on-device.
// =============================================================================

namespace services::timezone {

/** Fetch the UTC offset for this IP. Call once after WiFi connects. */
void init();

/** Current offset from UTC in seconds (e.g. -14400 for EDT). 0 until fetched. */
long offsetSeconds();

/** Format a UTC instant as local clock time, e.g. "6:00 PM", into out. */
void formatLocalTime(time_t utc, char* out, size_t cap);

/** Countdown from now to a UTC instant: "1h 48m" or "12m". */
void formatCountdown(time_t utc, time_t now, char* out, size_t cap);

}  // namespace services::timezone
