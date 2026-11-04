#pragma once

#include <cstdint>

// Tone curve for this panel, from the SD card: /.crosspoint/panel_curve.txt holds 256 integers 0..255
// (whitespace or comma separated) — wanted on-screen tone → value fed to the 4-gray ditherer, the
// inverse of the measured response. Lets images be prepared flat, as for a computer screen.
namespace panelcurve {
constexpr char PATH[] = "/.crosspoint/panel_curve.txt";

// Loads the curve; false (and `table` untouched) when the file is missing or malformed.
// `hash` identifies the curve for cache keys.
bool load(uint8_t table[256], uint32_t& hash);
}  // namespace panelcurve
