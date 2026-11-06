#include "HalFrontlight.h"

#include <Logging.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

HalFrontlight HalFrontlight::instance;

namespace {
#if FREEINK_FRONTLIGHT_DITHER
// 4 kHz sampling of the two channel accumulators. Rare fractional pulses can repeat more slowly than this timer.
constexpr uint64_t kRampStepUs = 250;
#else
constexpr uint64_t kRampStepUs = 20000;  // one LEDC write per step
#endif
#if FREEINK_DEVICE_X4PRO
constexpr bool kRampSupported = true;  // LEDC light on X4 Pro: writes from the timer task are plain register writes
#else
constexpr bool kRampSupported = false;  // I2C/PMIC lights elsewhere: keep their writes on the caller's task
#endif

class Guard {
 public:
  explicit Guard(void* m) : mutex(static_cast<SemaphoreHandle_t>(m)) {
    locked = mutex != nullptr && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
  }
  ~Guard() {
    if (locked) xSemaphoreGive(mutex);
  }

 private:
  SemaphoreHandle_t mutex;
  bool locked;
};

// The duty the SDK writes for an 8-bit level (FrontlightManager::apply) and for a percent (its curve).
uint32_t fullDuty() { return (1u << BoardConfig::ACTIVE.frontlight.pwmResolutionBits) - 1u; }
float levelDuty(const uint8_t level) {
  if (level == 0) return 0.0f;
  const uint32_t n = level - 1u;
  return static_cast<float>(1u + (n * n * (fullDuty() - 1u)) / (254u * 254u));
}
float percentDutyF(const uint8_t percent) {
  return static_cast<float>(FrontlightManager::percentDuty(percent, fullDuty()));
}
}  // namespace

void HalFrontlight::begin(const uint8_t brightness, const uint8_t warmth, const bool on) {
  // begin() runs the hardware probe (EEGO A4: an I2C ACK from the LM3630A at
  // 0x36) that decides present(), so it must come FIRST. Guarding on present()
  // before begin() would skip the probe forever and hide the light on units
  // that actually have one. begin() is inert on boards without a frontlight.
  manager.begin();
  if (!manager.present()) return;

  if (kRampSupported) {
    if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    if (rampTimer == nullptr && mutex != nullptr) {
      esp_timer_create_args_t args{};
      args.callback = &HalFrontlight::rampTick;
      args.arg = this;
      args.dispatch_method = ESP_TIMER_TASK;
      args.name = "krinik_light";
      esp_timer_handle_t timer = nullptr;
      if (esp_timer_create(&args, &timer) == ESP_OK) rampTimer = timer;
    }
  }

  Guard guard(mutex);
  lastBrightness = brightness > 100 ? 100 : brightness;
  lit = on;
  if (lit && rampTimer != nullptr && rampUs > 0) {
    // Wake/boot: the light comes up from dark while the boot goes on.
    manager.setBrightness(0);
    manager.setColorTemperature(warmth > 100 ? 100 : warmth);
    outLevel = 0;
    outDutyF = 0.0f;
    startRampLocked(levelFor(lastBrightness), lastBrightness);
  } else {
    // Brightness first: warmth applied at brightness 0 is an off write, which
    // puts the Picco LED driver in standby right before the on write.
    manager.setBrightness(lit ? lastBrightness : 0);
    manager.setColorTemperature(warmth > 100 ? 100 : warmth);
    outLevel = lit ? levelFor(lastBrightness) : 0;
    outDutyF = lit ? percentDutyF(lastBrightness) : 0.0f;
  }
  updateDitherTimerLocked();
  LOG_INF("LIGHT", "Frontlight up: %u%% warm=%u%% %s", lastBrightness, manager.colorTemperature(), lit ? "on" : "off");
}

void HalFrontlight::setBrightness(const uint8_t percent) {
  Guard guard(mutex);
  lastBrightness = percent > 100 ? 100 : percent;
  if (lit) {
    cancelRampLocked();  // a slider move during a ramp: the slider wins at once
    manager.setBrightness(lastBrightness);
    outLevel = levelFor(lastBrightness);
    outDutyF = percentDutyF(lastBrightness);
    updateDitherTimerLocked();
  }
}

void HalFrontlight::setWarmth(const uint8_t warmPercent) {
  Guard guard(mutex);
  manager.setColorTemperature(warmPercent > 100 ? 100 : warmPercent);
  updateDitherTimerLocked();
}

uint8_t HalFrontlight::levelFor(const uint8_t percent) {
  if (percent == 0) return 0;
  // The brightest 8-bit level whose duty does not exceed the percent's duty, in the SDK's own integer forms
  // (FrontlightManager::apply: percent → perceptualDuty, level → 1 + (L-1)²·(full-1)/254²), so a fade starts with no
  // step up. The percent's duty comes from the SDK itself, so any brightness curve stays in step.
  const uint32_t full = (1u << BoardConfig::ACTIVE.frontlight.pwmResolutionBits) - 1u;
  uint32_t percentDuty = FrontlightManager::percentDuty(percent, full);
  if (percentDuty == 0) percentDuty = 1;
  uint8_t best = 1;
  for (uint32_t level = 1; level <= 255; ++level) {
    const uint32_t n = level - 1u;
    if (1u + (n * n * (full - 1u)) / (254u * 254u) > percentDuty) break;
    best = static_cast<uint8_t>(level);
  }
  return best;
}

uint8_t HalFrontlight::currentOutputLevel() const { return manager.present() && lit ? outLevel : 0; }

void HalFrontlight::setOutputLevel(const uint8_t level) {
  if (!manager.present()) return;
  Guard guard(mutex);
  cancelRampLocked();
  manager.setBrightnessLevel(level);
  outLevel = level;
  outDutyF = levelDuty(level);
  updateDitherTimerLocked();
}

void HalFrontlight::rampOutputTo(const uint8_t level) {
  if (!manager.present()) return;
  Guard guard(mutex);
  if (rampTimer != nullptr && rampUs > 0) {
    startRampLocked(level, -1);
    return;
  }
  cancelRampLocked();
  manager.setBrightnessLevel(level);
  outLevel = level;
  outDutyF = levelDuty(level);
  updateDitherTimerLocked();
}

void HalFrontlight::restoreOutput(const bool smooth) {
  if (!manager.present()) return;
  Guard guard(mutex);
  if (smooth && rampTimer != nullptr && rampUs > 0) {
    startRampLocked(lit ? levelFor(lastBrightness) : 0, lit ? lastBrightness : 0);
    return;
  }
  cancelRampLocked();
  manager.setBrightness(lit ? lastBrightness : 0);
  outLevel = lit ? levelFor(lastBrightness) : 0;
  outDutyF = lit ? percentDutyF(lastBrightness) : 0.0f;
  updateDitherTimerLocked();
}

void HalFrontlight::prepareForDeepSleep() {
  Guard guard(mutex);
  cancelRampLocked();
  manager.prepareForDeepSleep();
}

void HalFrontlight::setOn(const bool on) {
  Guard guard(mutex);
  if (on == lit) return;
  lit = on;
  if (rampTimer != nullptr && rampUs > 0) {
    startRampLocked(on ? levelFor(lastBrightness) : 0, on ? lastBrightness : 0);
    return;
  }
  manager.setBrightness(lit ? lastBrightness : 0);
  outLevel = lit ? levelFor(lastBrightness) : 0;
  outDutyF = lit ? percentDutyF(lastBrightness) : 0.0f;
  updateDitherTimerLocked();
}

void HalFrontlight::startRampLocked(const uint8_t toLevel, const int16_t finalPercent) {
  auto* timer = static_cast<esp_timer_handle_t>(rampTimer);
  rampFrom = outLevel;  // from where the LEDs are now: a toggle mid-ramp turns around without a jump
  rampTo = toLevel;
  rampFromDutyF = outDutyF;
  rampToDutyF = finalPercent >= 0 ? percentDutyF(static_cast<uint8_t>(finalPercent)) : levelDuty(toLevel);
  rampFinalPercent = finalPercent;
  rampStartUs = esp_timer_get_time();
  ramping = true;
  esp_timer_stop(timer);  // not running → ESP_ERR_INVALID_STATE, harmless
  timerRunning = esp_timer_start_periodic(timer, kRampStepUs) == ESP_OK;
  if (!timerRunning) {
    ramping = false;  // no timer: switch at once, as on other boards
    if (finalPercent >= 0) {
      manager.setBrightness(static_cast<uint8_t>(finalPercent));
    } else {
      manager.setBrightnessLevel(toLevel);
    }
    outLevel = rampTo;
    outDutyF = rampToDutyF;
  }
}

void HalFrontlight::cancelRampLocked() {
  ramping = false;
  if (timerRunning) {
    esp_timer_stop(static_cast<esp_timer_handle_t>(rampTimer));
    timerRunning = false;
  }
}

void HalFrontlight::updateDitherTimerLocked() {
#if FREEINK_FRONTLIGHT_DITHER && FREEINK_DEVICE_X4PRO
  if (rampTimer == nullptr || ramping) return;
  if (!manager.needsPwmDither()) {
    cancelRampLocked();
  } else if (!timerRunning) {
    timerRunning = esp_timer_start_periodic(static_cast<esp_timer_handle_t>(rampTimer), kRampStepUs) == ESP_OK;
  }
#endif
}

void HalFrontlight::rampTick(void* self) {
  auto& light = *static_cast<HalFrontlight*>(self);
  Guard guard(light.mutex);
  if (!light.timerRunning) return;  // cancelled or parked while this tick waited for the lock
  if (!light.ramping) {
#if FREEINK_FRONTLIGHT_DITHER && FREEINK_DEVICE_X4PRO
    light.manager.setTotalDuty(light.outDutyF);
    light.updateDitherTimerLocked();
#endif
    return;
  }
  const int64_t t = esp_timer_get_time() - light.rampStartUs;
  if (t >= light.rampUs || light.rampUs <= 0) {
    light.cancelRampLocked();
    // End exactly on the target: the saved percent (the SDK's percent curve) for on/off, else the raw level.
    if (light.rampFinalPercent >= 0) {
      light.manager.setBrightness(static_cast<uint8_t>(light.rampFinalPercent));
    } else {
      light.manager.setBrightnessLevel(light.rampTo);
    }
    light.outLevel = light.rampTo;
    light.outDutyF = light.rampToDutyF;
    light.updateDitherTimerLocked();
    return;
  }
  const int from = light.rampFrom;
  const int to = light.rampTo;
#if FREEINK_FRONTLIGHT_DITHER
  // Preserve the fractional total until the SDK mixes and quantizes the two channels.
  (void)from;
  (void)to;
  const float k = static_cast<float>(t) / static_cast<float>(light.rampUs);
  const float s0 = sqrtf(light.rampFromDutyF);
  const float s1 = sqrtf(light.rampToDutyF);
  const float sNow = s0 + (s1 - s0) * k;
  const float dutyF = sNow * sNow;
  light.manager.setTotalDuty(dutyF);
  light.outDutyF = dutyF;
  light.outLevel = static_cast<uint8_t>(light.rampFrom + (light.rampTo - light.rampFrom) * k + 0.5f);
#else
  const int level = from + static_cast<int>((to - from) * t / light.rampUs);
  light.manager.setBrightnessLevel(static_cast<uint8_t>(level));
  light.outLevel = static_cast<uint8_t>(level);
#endif
}
