#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // Guaranteed clean (Nikolai, 04.11.2026): two scrubs of opposite polarity. A HALF on this reader's UC8279 seeds the
  // OLD plane with the complement of the target, so EVERY pixel — background included — goes through a transition:
  // first the whole panel to black, then to white. Two full passes cleared the band under the menu header (one did
  // not); black-then-white keeps the drive DC-balanced, where two passes to white did not. FULL seeds OLD white and
  // leaves the background idle (ghosts).
  LOG_DBG("CLN", "Clean: scrub to black, scrub to white");
  renderer.clearScreen(0x00);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
