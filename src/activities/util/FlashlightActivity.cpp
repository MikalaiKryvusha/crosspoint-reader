#include "FlashlightActivity.h"

#include <GfxRenderer.h>
#include <HalFrontlight.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"

void FlashlightActivity::onEnter() {
  Activity::onEnter();
#ifndef SIMULATOR
  Frontlight.setOutputLevel(255);
#endif
  requestUpdate();
}

void FlashlightActivity::onExit() {
#ifndef SIMULATOR
  Frontlight.restoreOutput();
#endif
  Activity::onExit();
}

void FlashlightActivity::render(RenderLock&&) {
  // Keep the page white in either UI polarity; the header supplies the shared Back target.
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen(0xFF);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_FLASHLIGHT_EXIT));
  // The render task applies night-mode polarity globally.
  if (SETTINGS.screenInverted) renderer.invertScreen();
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
