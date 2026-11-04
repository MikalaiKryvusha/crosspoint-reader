#pragma once
#include "activities/Activity.h"

// Home → «Фонарик» (Nikolai 04.11.2026): the whole screen white and the frontlight at full output. A tap (or Back /
// OK) returns Home; leaving by any way — Home, sleep — puts the light back as it was: the saved brightness, or dark if
// it was off. The saved brightness and the on/off setting are never touched.
class FlashlightActivity final : public Activity {
 public:
  explicit FlashlightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Flashlight", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
