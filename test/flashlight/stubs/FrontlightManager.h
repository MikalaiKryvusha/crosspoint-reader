#pragma once
#include <cstdint>
inline int physicalOutput = 0, rawLevel = 0, physicalWarmth = 50;
class FrontlightManager {
 public:
  void begin() {}
  bool present() const { return true; }
  bool hasColorTemperature() const { return true; }
  void setBrightness(uint8_t p) {
    physicalOutput = p;
    rawLevel = -1;
  }
  void setBrightnessLevel(uint8_t p) {
    rawLevel = p;
    physicalOutput = p ? 100 : 0;
  }
  void setColorTemperature(uint8_t p) { physicalWarmth = p; }
  uint8_t colorTemperature() const { return physicalWarmth; }
};
