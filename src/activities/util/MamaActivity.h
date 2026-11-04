#pragma once
#include "activities/Activity.h"

// Krinik «для мамы» build: Home → «♥ Для любимой Мамы» opens a heart and a dedication. Any button returns Home.
// Drawn in render(), so night mode (output polarity set per render by ActivityManager) applies as everywhere.
class MamaActivity final : public Activity {
 public:
  explicit MamaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Mama", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
