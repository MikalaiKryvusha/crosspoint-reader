#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // One HALF to white. On this reader's UC8279 it is the driver's scrub (OLD = complement of the target, every pixel
  // driven), and after a gray image the driver adds its own exit-from-gray paint first. That pair cleared the band
  // under the menu header (device, 04.11.2026); a balanced black+white double scrub brought it back and took 4x as long.
  LOG_DBG("CLN", "Clean: HALF scrub to white");
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
