#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void shortClean(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // One HALF to white — on screen the waveform itself flashes black, then white. On this reader's UC8279 it is the
  // driver's scrub (OLD = complement of the target, every pixel driven); over a gray image the driver adds its
  // exit-from-gray paint first. With the remnant drained this plus the gray repaint gave a band-free image
  // (device, 04.11.2026, three 2-minute rests).
  LOG_DBG("CLN", "Short clean: HALF scrub to white");
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
