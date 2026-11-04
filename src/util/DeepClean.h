#pragma once

class GfxRenderer;

// Deep clean before an absolute-gray image on X4 Pro (SSD1677): two black→white cycles, the last white a FULL
// refresh. A single clean leaves a faint ghost of what stood on the panel for long (the 84 px header band of the
// menus showed through gray sleep and viewer images ~1 cm from the top); a weak gray waveform cannot overdrive it,
// full black/white flips can. Leaves the framebuffer white. No-op on other boards.
namespace deepclean {
void toWhite(GfxRenderer& renderer);
}  // namespace deepclean
