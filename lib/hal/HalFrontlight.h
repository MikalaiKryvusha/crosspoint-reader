#pragma once

#include <FrontlightManager.h>

// Thin firmware HAL over the SDK frontlight manager. It is inert on boards
// without a frontlight, so callers do not need board-specific conditionals.
class HalFrontlight {
 public:
  static HalFrontlight& getInstance() { return instance; }

  void begin(uint8_t brightness, uint8_t warmth, bool on);

  bool present() const { return manager.present(); }
  bool hasColorTemperature() const { return manager.hasColorTemperature(); }

  void setBrightness(uint8_t percent);
  void setWarmth(uint8_t warmPercent);
  void setOn(bool on);
  // Fade support (sleep light linger): the 8-bit perceptual output level (SDK setBrightnessLevel, gamma 2) that gives
  // the same duty as the current brightness percent (gamma 1.6554), so a fade starts without a jump; and a raw output
  // write that leaves the saved brightness and the on/off state untouched. 0 = dark.
  uint8_t currentOutputLevel() const;
  void setOutputLevel(uint8_t level);
  // Back to the saved state after setOutputLevel: the saved brightness when on, dark when off.
  void restoreOutput();
  // Park the PWM pads at the LED-off level and hold them through deep sleep (the saved light state is untouched;
  // begin() releases the hold on the next full boot; the settled-redraw timer path and the SD-error path never call
  // it, so there the pads stay parked — the light stays off).
  void prepareForDeepSleep() { manager.prepareForDeepSleep(); }

  uint8_t brightness() const { return lastBrightness; }
  uint8_t warmth() const { return manager.colorTemperature(); }
  bool isOn() const { return lit; }

 private:
  HalFrontlight() = default;

  FrontlightManager manager;
  // The SDK represents off as brightness 0. Keep the selected brightness so
  // toggling back on restores it.
  uint8_t lastBrightness = 60;
  bool lit = false;

  static HalFrontlight instance;
};

#define Frontlight HalFrontlight::getInstance()
