#include "DeepClean.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <Logging.h>

namespace deepclean {
void toWhite(GfxRenderer& renderer) {
#if FREEINK_DEVICE_X4PRO
  constexpr int CYCLES = 2;
  LOG_DBG("CLN", "Deep clean: %d black/white cycles", CYCLES);
  for (int i = 0; i < CYCLES; ++i) {
    // Non-differential steps only: a FAST (DU) update drives just the pixels that change, so whatever was already
    // black (the «Загрузка» popup, the menu header) kept its own history and ghosted through the gray paint.
    // HALF and FULL bypass the previous-image plane and drive every pixel the same way.
    renderer.clearScreen(0x00);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    renderer.clearScreen(0xFF);
    renderer.displayBuffer(i == CYCLES - 1 ? HalDisplay::FULL_REFRESH : HalDisplay::HALF_REFRESH);
  }
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
