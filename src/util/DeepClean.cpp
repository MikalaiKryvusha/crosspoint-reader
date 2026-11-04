#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // One single-pass clean to white (HALF, 0xD7), as stock paints its sleep screens. FULL (0xF7) is the multi-flash
  // OTP waveform (several black/white inversions, ~1.8 s); extra cycles did not help — the faint band on gray images
  // is the panel itself (it shows on stock CrossPoint 2.0.0 too, research/gray_stripe.md).
  LOG_DBG("CLN", "Clean to white (HALF) before absolute gray");
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
