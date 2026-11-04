#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // One FULL clean to white (0xF7, the OTP multi-flash waveform: several black/white inversions, ~1.8 s) — Nikolai's
  // choice over the single-pass HALF that stock uses for sleep screens. Extra cycles did not help: the faint band on
  // gray images is the panel itself (it shows on stock CrossPoint 2.0.0 too, research/gray_stripe.md).
  LOG_DBG("CLN", "Clean to white (FULL) before absolute gray");
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
