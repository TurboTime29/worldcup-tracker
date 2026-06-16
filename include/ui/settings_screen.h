#pragma once

// =============================================================================
// On-device settings screen — opened by holding the touchscreen ~5s.
// Adjusts display brightness with on-screen − / + buttons; DONE returns to the
// match view. Brightness is applied live and persisted by the caller.
// =============================================================================

namespace ui::settings {

enum class Hit { None, Minus, Plus, Done };

/** Draw the settings screen showing the current brightness percentage. */
void draw(int percent);

/** Map a tap location to a button (or None). */
Hit hitTest(int x, int y);

}  // namespace ui::settings
