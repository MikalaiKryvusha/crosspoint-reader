#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
inline unsigned long fakeNow = 0;
inline unsigned long millis() { return fakeNow; }
inline void delay(unsigned long ms) { fakeNow += ms; }
inline uint32_t pwm[2] = {};
inline unsigned pwmWrites = 0;
inline bool pwmDetached[2] = {};
inline void ledcSetup(uint8_t, uint32_t, uint8_t) {}
inline void ledcAttachPin(int8_t, uint8_t ch) { pwmDetached[ch] = false; }
inline void ledcDetachPin(int8_t gpio) { pwmDetached[gpio == 9 ? 1 : 0] = true; }
inline void ledcWrite(uint8_t ch, uint32_t duty) {
  pwm[ch] = duty;
  ++pwmWrites;
}
constexpr int HIGH = 1, LOW = 0, OUTPUT = 3;
inline int Serial = 0;
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int level) {
  if (pin == 8 || pin == 9) pwm[pin == 9 ? 1 : 0] = level;
}
