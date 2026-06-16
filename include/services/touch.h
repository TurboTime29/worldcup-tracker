#pragma once

namespace services::touch {

enum class Event { None, Tap, SwipeLeft, SwipeRight };

/** Reset the CST816 controller and start the I2C bus. Call once in setup(). */
void init();

/**
 * Poll the controller. Returns at most one event per finger gesture:
 *   - Tap        : quick press/release with little movement
 *   - SwipeLeft  : finger moved right->left  (advance to next match)
 *   - SwipeRight : finger moved left->right  (previous match)
 * Call frequently from loop().
 */
Event poll();

/** Screen coordinates (0..239) of the last Tap event (used by the settings UI). */
int tapX();
int tapY();

}  // namespace services::touch
