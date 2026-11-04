#pragma once

class GfxRenderer;

// Guaranteed clean on X4 Pro: scrub every pixel to black, then to white (DC-balanced). Before gray images, on leaving
// them, after a theme switch. Leaves the framebuffer white. No-op elsewhere.
namespace deepclean {
void toWhite(GfxRenderer& renderer);
}  // namespace deepclean
