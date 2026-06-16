#pragma once

#include <cstddef>

#include "model.h"

// =============================================================================
// Renders one round match screen (mockup-matched): stage label, status pill,
// team columns, score / VS+kickoff, footer, time ring, and page dots.
// =============================================================================

namespace ui::match {

/** Draw the match at `index` of `total` (for the page dots + ring). */
void draw(const model::Match& m, size_t index, size_t total);

/** Centered message screen, e.g. "No matches today" / "Open setup". */
void drawMessage(const char* line1, const char* line2);

}  // namespace ui::match
