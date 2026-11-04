#include "FlashlightActivity.h"

#include <GfxRenderer.h>
#include <HalFrontlight.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"

void FlashlightActivity::onEnter() {
  Activity::onEnter();
#ifndef SIMULATOR  // the simulator HAL (external dependency, not ours to edit) has no raw light output
  Frontlight.rampOutputTo(255);  // full output over «Плавная подсветка»; saved brightness and on/off untouched
#endif
  requestUpdate();
}

void FlashlightActivity::onExit() {
#ifndef SIMULATOR
  Frontlight.restoreOutput(/*smooth=*/true);  // back to the saved brightness (or dark) over the same ramp
#endif
  Activity::onExit();
}

void FlashlightActivity::render(RenderLock&&) {
  // White page with the usual header band on top: back arrow + «Выйти», so the way out is plain (Nikolai
  // 04.11.2026). The rest of the glass stays white for light; a tap anywhere still leaves.
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen(0xFF);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Выйти");
  // Night mode inverts the whole output (the render task applies it before render()): pre-invert the buffer so the
  // glass shows this page as drawn — white, black header text — in either theme.
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
