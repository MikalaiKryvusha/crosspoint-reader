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
    renderer.clearScreen(0x00);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    renderer.clearScreen(0xFF);
    renderer.displayBuffer(i == CYCLES - 1 ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  }
#else
  (void)renderer;
#endif
}
}  // namespace deepclean
