#pragma once

class GfxRenderer;

// X4 Pro gray ghosts are remnant voltage of frames drawn seconds before a gray image (research/gray_stripe.md,
// 04.11.2026): cleans right before the gray do not remove it, time does. So the old call sites (before gray images,
// leaving them, closing a sheet) no longer clean — toWhite() is a no-op there. The clean runs only in a settled
// redraw (viewer after 2 min, sleep image after 5 min) and in the optional cleans (sheet close, leaving a viewer
// image for the menus). Leaves the framebuffer white.
namespace deepclean {
void toWhite(GfxRenderer& renderer);        // former call sites: no-op
void shortClean(GfxRenderer& renderer);  // one HALF to white (flashes black, then white)
}  // namespace deepclean
