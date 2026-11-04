#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  // HALF before the gray paint (Nikolai's trial, 04.11.2026). On this reader's UC8279 a Half is the driver's charge
  // scrub — the OLD plane is seeded with the complement of the target, so every pixel (the white background too)
  // goes through a transition; its Full seeds OLD white and leaves the background idle, which kept ghosts (the
  // control center, the menu header line) under gray images.
  LOG_DBG("CLN", "Clean to white (HALF scrub) before absolute gray");
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
