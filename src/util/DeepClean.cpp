#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // One FULL clean to white. Two black/white cycles (FAST, then HALF) did not remove the band on the device: the cause
  // was the gray paint starting before the panel rails settled (fixed in the SSD1677 driver), not too little cleaning.
  LOG_DBG("CLN", "Clean to white (FULL) before absolute gray");
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
