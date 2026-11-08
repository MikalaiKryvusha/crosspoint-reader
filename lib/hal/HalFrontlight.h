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
  // Fractional temperature mixing is confined to the lowest 10% of the brightness curve.
  void setPreciseTemperature(bool enabled);
  // On X4 Pro the switch is a ramp (setRampMs) on the perceptual curve (Nikolai 04.11.2026: «плавной ШИМ анимацией»),
  // and so is the light coming on at boot/wake (begin): driven by an esp_timer task — the caller (boot, the control
  // center, the power-button toggle) never waits for it. Other boards switch at once, as before.
  void setOn(bool on);
  // Ramp length in ms (Settings → Display → «Плавная подсветка»); 0 = switch at once.
  void setRampMs(uint32_t ms) { rampUs = static_cast<int64_t>(ms) * 1000; }
  // Fade support (sleep light linger): the 8-bit perceptual output level (SDK setBrightnessLevel, gamma 2) that gives
  // the same duty as the current brightness percent (gamma 1.6554), so a fade starts without a jump; and a raw output
  // write that leaves the saved brightness and the on/off state untouched. 0 = dark.
  uint8_t currentOutputLevel() const;
  void setOutputLevel(uint8_t level);
  // The same raw output, reached over the «Плавная подсветка» ramp from wherever the LEDs are now (at once when the
  // ramp is off or unsupported). «Фонарик» enters with it.
  void rampOutputTo(uint8_t level);
  // Back to the saved state after setOutputLevel: the saved brightness when on, dark when off. smooth: over the ramp.
  void restoreOutput(bool smooth = false);
  // Park the PWM pads at the LED-off level and hold them through deep sleep (the saved light state is untouched;
  // begin() releases the hold on the next full boot; the settled-redraw timer path and the SD-error path never call
  // it, so there the pads stay parked — the light stays off).
  void prepareForDeepSleep();

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
  bool preciseTemperature = true;

  // The ramp. Every write to the manager goes under `mutex`: the timer task and the main/input tasks share it.
  static void rampTick(void* self);
  static uint8_t levelFor(uint8_t percent);
  // finalPercent ≥ 0: the ramp ends exactly on that percent (SDK percent curve); −1: it ends on the level itself.
  void startRampLocked(uint8_t toLevel, int16_t finalPercent);
  void cancelRampLocked();
  void updateDitherTimerLocked();
  void* rampTimer = nullptr;  // esp_timer_handle_t, created in begin() on X4 Pro
  void* mutex = nullptr;      // SemaphoreHandle_t
  int64_t rampStartUs = 0;
  int64_t rampUs = 200000;
  uint8_t rampFrom = 0;
  uint8_t rampTo = 0;
  uint8_t outLevel = 0;  // the level on the LEDs now (8-bit perceptual), for a ramp or fade to start from
  bool ramping = false;
  bool timerRunning = false;
  int16_t rampFinalPercent = -1;
  float outDutyF = 0.0f;       // the exact total duty on the LEDs now: a fade starts from it, no jump
  float rampFromDutyF = 0.0f;  // the ramp's exact duty endpoints (FREEINK_FRONTLIGHT_DITHER)
  float rampToDutyF = 0.0f;

  static HalFrontlight instance;
};

#define Frontlight HalFrontlight::getInstance()
