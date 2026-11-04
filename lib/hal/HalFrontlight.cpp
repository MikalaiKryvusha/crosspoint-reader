#include "HalFrontlight.h"

#include <Logging.h>
#include <math.h>

HalFrontlight HalFrontlight::instance;

void HalFrontlight::begin(const uint8_t brightness, const uint8_t warmth, const bool on) {
  // begin() runs the hardware probe (EEGO A4: an I2C ACK from the LM3630A at
  // 0x36) that decides present(), so it must come FIRST. Guarding on present()
  // before begin() would skip the probe forever and hide the light on units
  // that actually have one. begin() is inert on boards without a frontlight.
  manager.begin();
  if (!manager.present()) return;

  lastBrightness = brightness > 100 ? 100 : brightness;
  lit = on;
  // Brightness first: warmth applied at brightness 0 is an off write, which
  // puts the Picco LED driver in standby right before the on write.
  manager.setBrightness(lit ? lastBrightness : 0);
  manager.setColorTemperature(warmth > 100 ? 100 : warmth);
  LOG_INF("LIGHT", "Frontlight up: %u%% warm=%u%% %s", lastBrightness, manager.colorTemperature(), lit ? "on" : "off");
}

void HalFrontlight::setBrightness(const uint8_t percent) {
  lastBrightness = percent > 100 ? 100 : percent;
  if (lit) manager.setBrightness(lastBrightness);
}

void HalFrontlight::setWarmth(const uint8_t warmPercent) {
  manager.setColorTemperature(warmPercent > 100 ? 100 : warmPercent);
}

uint8_t HalFrontlight::currentOutputLevel() const {
  if (!manager.present() || !lit || lastBrightness == 0) return 0;
  // percent curve: duty/full = (p/100)^1.6554; level curve: duty/full ≈ ((L-1)/254)^2  →  L = 1 + 254·(p/100)^0.8277
  const float level = 1.0f + 254.0f * powf(lastBrightness / 100.0f, 1.6554f / 2.0f);
  return level >= 255.0f ? 255 : static_cast<uint8_t>(level + 0.5f);
}

void HalFrontlight::setOutputLevel(const uint8_t level) {
  if (manager.present()) manager.setBrightnessLevel(level);
}

void HalFrontlight::setOn(const bool on) {
  if (on == lit) return;
  lit = on;
  manager.setBrightness(lit ? lastBrightness : 0);
}
