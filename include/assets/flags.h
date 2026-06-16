#pragma once

#include <cstddef>
#include <cstdint>

// AUTO-GENERATED companion: see scripts/build_flags.py

namespace assets {

struct Flag { const char* code; const uint16_t* data; uint16_t w; uint16_t h; };
extern const Flag kFlags[];
extern const size_t kFlagCount;

/** RGB565 flag for an API 3-letter code (w/h filled), or nullptr. */
const uint16_t* flagImage(const char* code, uint16_t* w, uint16_t* h);

}  // namespace assets
