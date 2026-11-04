#include "FlashlightActivity.h"

#include <GfxRenderer.h>
#include <HalFrontlight.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"

void FlashlightActivity::onEnter() {
  Activity::onEnter();
  Frontlight.setOutputLevel(255);  // full output; the saved brightness and on/off state stay as they are
  requestUpdate();
}

void FlashlightActivity::onExit() {
  Frontlight.restoreOutput();
  Activity::onExit();
}

void FlashlightActivity::render(RenderLock&&) {
  // White on the glass in either theme: night mode inverts the whole output (the render task applies it before
  // render()), so the buffer is filled black there.
  renderer.clearScreen(SETTINGS.screenInverted ? 0x00 : 0xFF);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void FlashlightActivity::loop() {
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y) || mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activityManager.goHome(HomeMenuItem::FLASHLIGHT);
  }
}
