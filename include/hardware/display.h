#pragma once

#include "hardware/lgfx_config.hpp"

extern LGFX tft;

void displayInit();

/** Set the backlight from a percentage (10..100). Persists nothing itself. */
void displaySetBrightnessPercent(int pct);
