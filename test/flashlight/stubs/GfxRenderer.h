#pragma once
#include <cstdint>
inline int panelWidth = 480, clearValue = 0, shownValue = 0;
inline bool panelInverted = false;
class RenderLock {};
class GfxRenderer {
 public:
  void clearScreen(uint8_t p) { clearValue = p; }
  void invertScreen() { clearValue ^= 255; }
  int getScreenWidth() const { return panelWidth; }
  void displayBuffer(int) { shownValue = clearValue ^ (panelInverted ? 255 : 0); }
};
struct HalDisplay {
  static constexpr int HALF_REFRESH = 1;
};
