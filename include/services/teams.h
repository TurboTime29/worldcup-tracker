#pragma once

#include <cstddef>

// =============================================================================
// Team directory: name <-> 3-letter FIFA code.
//
// football-data.org gives each team a `tla` (3-letter code) directly, so at
// runtime we mostly use that. This module provides:
//   - the canonical team list for the setup-portal dropdown, and
//   - a name->code fallback for any feed that omits the tla.
// =============================================================================

namespace services::teams {

struct TeamInfo {
  const char* name;  // display name, e.g. "Spain"
  const char* code;  // 3-letter FIFA code, e.g. "ESP"
};

/** Canonical, de-duplicated team list (for the dropdown), sorted by name. */
const TeamInfo* list();
size_t count();

/** Writes the 3-letter FIFA code for `name` into out[4] (NUL-terminated). */
void codeForName(const char* name, char out[4]);

}  // namespace services::teams
