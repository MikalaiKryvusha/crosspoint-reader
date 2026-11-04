#pragma once

#include <cstdint>

// Tone curve for this panel, from the SD card: /.crosspoint/panel_curve.txt holds 256 integers 0..255
// (whitespace or comma separated) — wanted on-screen tone → value fed to the 4-gray ditherer, the
// inverse of the measured response. Lets images be prepared flat, as for a computer screen.
namespace panelcurve {
constexpr char PATH[] = "/.crosspoint/panel_curve.txt";

// Loads the curve from the file, or the built-in one (this X4 Pro, measured 03.11.2026) when the file is
// missing or malformed. Always fills `table` and returns true; `hash` identifies the curve for cache keys.
// A file holding 0, 1, …, 255 switches the correction off.
bool load(uint8_t table[256], uint32_t& hash);
}  // namespace panelcurve
