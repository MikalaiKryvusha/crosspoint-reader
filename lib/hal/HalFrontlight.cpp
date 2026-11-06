#include "HalFrontlight.h"

#include <Logging.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

HalFrontlight HalFrontlight::instance;

namespace {
constexpr uint64_t kRampStepUs = 20000;    // one LEDC write per step
#if FREEINK_DEVICE_X4PRO
constexpr bool kRampSupported = true;      // LEDC light on X4 Pro: writes from the timer task are plain register writes
#else
constexpr bool kRampSupported = false;     // I2C/PMIC lights elsewhere: keep their writes on the caller's task
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
    startRampLocked(levelFor(lastBrightness), lastBrightness);
  } else {
    // Brightness first: warmth applied at brightness 0 is an off write, which
    // puts the Picco LED driver in standby right before the on write.
    manager.setBrightness(lit ? lastBrightness : 0);
    manager.setColorTemperature(warmth > 100 ? 100 : warmth);
    outLevel = lit ? levelFor(lastBrightness) : 0;
  }
  LOG_INF("LIGHT", "Frontlight up: %u%% warm=%u%% %s", lastBrightness, manager.colorTemperature(), lit ? "on" : "off");
}

void HalFrontlight::setBrightness(const uint8_t percent) {
  Guard guard(mutex);
  lastBrightness = percent > 100 ? 100 : percent;
  if (lit) {
    cancelRampLocked();  // a slider move during a ramp: the slider wins at once
    manager.setBrightness(lastBrightness);
    outLevel = levelFor(lastBrightness);
  }
}

void HalFrontlight::setWarmth(const uint8_t warmPercent) {
  Guard guard(mutex);
  manager.setColorTemperature(warmPercent > 100 ? 100 : warmPercent);
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
}

void HalFrontlight::startRampLocked(const uint8_t toLevel, const int16_t finalPercent) {
  auto* timer = static_cast<esp_timer_handle_t>(rampTimer);
  rampFrom = outLevel;  // from where the LEDs are now: a toggle mid-ramp turns around without a jump
  rampTo = toLevel;
  rampFinalPercent = finalPercent;
  rampStartUs = esp_timer_get_time();
  ramping = true;
  esp_timer_stop(timer);  // not running → ESP_ERR_INVALID_STATE, harmless
  if (esp_timer_start_periodic(timer, kRampStepUs) != ESP_OK) {
    ramping = false;  // no timer: switch at once, as on other boards
    if (finalPercent >= 0) {
      manager.setBrightness(static_cast<uint8_t>(finalPercent));
    } else {
      manager.setBrightnessLevel(toLevel);
    }
    outLevel = rampTo;
  }
}

void HalFrontlight::cancelRampLocked() {
  if (!ramping) return;
  ramping = false;
  esp_timer_stop(static_cast<esp_timer_handle_t>(rampTimer));
}

void HalFrontlight::rampTick(void* self) {
  auto& light = *static_cast<HalFrontlight*>(self);
  Guard guard(light.mutex);
  if (!light.ramping) return;  // cancelled while this tick waited for the lock
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
    return;
  }
  const int from = light.rampFrom;
  const int to = light.rampTo;
  const int level = from + static_cast<int>((to - from) * t / light.rampUs);
  light.manager.setBrightnessLevel(static_cast<uint8_t>(level));
  light.outLevel = static_cast<uint8_t>(level);
}
