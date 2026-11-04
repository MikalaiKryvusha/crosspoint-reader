#pragma once

#include <GfxRenderer.h>

#include <cstdint>
#include <functional>
#include <string>

#include "MappedInputManager.h"
#include "activities/Activity.h"

class Bitmap;

// Image viewer (BMP, JPEG, PNG). Every image is shown in gray through the panel tone curve: it is prepared once into
// a screen-ready 2-bit BMP cache (/.crosspoint/view_*.bmp) while «Загрузка» stands on a white sheet, then the panel
// is deep-cleaned and the gray image painted from the cache. Prev/next by buttons, swipes and side taps, wrapping
// around the folder; a long tap rotates the view by 90°. The curve is loaded once and kept in RAM while open.
class BmpViewerActivity final : public Activity {
 public:
  BmpViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string filePath);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  void loadSiblingImages();
  void doSetSleepCover();
  bool canSetSleepCover() const;
  bool saveJpegSleepCover();
  void showCurrent();
  void showMessage(const char* message);
  bool prepareCache(std::string& outPath);
  bool paintBitmap(Bitmap& bitmap);
  const char* paintCurrent();
  void openSibling(int delta);
  void rotate();
  // The picture, touches, the control center and messages: the reading orientation, turned by long-tap rotations.
  GfxRenderer::Orientation drawOrientation() const;
  GfxRenderer::Orientation holdOrientation() const;

  std::string filePath;
  std::vector<std::string> siblingImages;
  int currentImageIndex = -1;

  uint8_t curve[256] = {};
  uint32_t curveHash = 0;
  bool curveReady = false;
  bool orientationSaved = false;
  int viewTurns = 0;          // long-tap quarter turns, clockwise
  uint32_t paintedVisit = 0;  // screen visit of the last paint; a newer one means a sheet covered the image
  GfxRenderer::Orientation savedOrientation = GfxRenderer::Orientation::Portrait;
};
