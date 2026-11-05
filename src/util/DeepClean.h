#pragma once

class GfxRenderer;

// X4 Pro gray ghosts are remnant voltage of frames drawn seconds before a gray image; it drains in minutes, and a
// clean right before the gray does not remove it (research/gray_stripe.md, 04.11.2026) — the settled redraws do
// («Обновлять картинки» / «Обновлять заставку»: 1 / 2 / 5 min). This short clean serves the white sheet before the
// sleep image and the optional clean after the control center closes.
namespace deepclean {
// One HALF to white (on screen it flashes black, then white); in night mode the output inversion makes it black.
// Leaves the framebuffer white. No-op on other boards.
void shortClean(GfxRenderer& renderer);
}  // namespace deepclean
