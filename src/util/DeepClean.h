#pragma once

class GfxRenderer;

// Clean before an absolute-gray image on X4 Pro (SSD1677): one FULL refresh to white, so the previous screen does
// not show through and no harsh B/W draft flashes before the grays. Leaves the framebuffer white. No-op elsewhere.
namespace deepclean {
void toWhite(GfxRenderer& renderer);
}  // namespace deepclean
