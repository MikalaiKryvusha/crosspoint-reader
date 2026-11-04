#pragma once
#include <string>

#include "activities/Activity.h"

class Bitmap;
class HalFile;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;

  // Settled redraw of the own («Custom») sleep image drawn by the last sleep: a short clean, then the same image.
  // False when the last sleep drew no own image. Called from the 5-minute nap before deep sleep (main.cpp).
  static bool redrawLastCustomImage(GfxRenderer& renderer, MappedInputManager& mappedInput);

 private:
  bool redrawCustomImage(const std::string& path) const;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  bool renderCustomSleepBmp(const std::string& path) const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap, bool preserveBackground = false) const;
  bool renderSleepOverlayFile(HalFile& file, const char* pathForLog) const;
  bool renderTransparentOverlayPng(const std::string& path) const;
  bool renderSleepOverlayPath(const std::string& path) const;
  void renderLastScreenSleepScreen() const;
  void renderTransparentCustomSleepScreen() const;
  void renderBlankSleepScreen() const;

  bool fromTimeout = false;
  // Cover + Transparent: the cover under the overlay, and the same cover while its gray planes are redrawn.
  mutable const Bitmap* overlayCover = nullptr;
  mutable const Bitmap* overlayGrayCover = nullptr;
};
