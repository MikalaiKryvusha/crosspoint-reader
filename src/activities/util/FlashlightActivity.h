#pragma once

#include "activities/Activity.h"

// Temporary white screen and full frontlight output. Exit restores the saved light state.
class FlashlightActivity final : public Activity {
 public:
  explicit FlashlightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Flashlight", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
