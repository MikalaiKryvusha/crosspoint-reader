#include <HalFrontlight.h>
#include <esp_timer.h>

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
#if FREEINK_DEVICE_X4PRO
void tick() {
  assert(fakeTimer.active);
  fakeTimeUs += fakeTimer.period;
  fakeTimer.args.callback(fakeTimer.args.arg);
  assert(pwm[0] <= 2047 && pwm[1] <= 2047);
}
#if FREEINK_FRONTLIGHT_DITHER
void neutralTicks(unsigned count) {
  for (unsigned i = 0; i < count; ++i) {
    tick();
    assert(pwm[0] == pwm[1]);
  }
}
void mean(unsigned samples, double cool, double warm) {
  uint64_t c = 0, w = 0;
  for (unsigned i = 0; i < samples; ++i) {
    tick();
    c += pwm[0];
    w += pwm[1];
  }
  assert(std::abs(static_cast<double>(c) - samples * cool) < 1.1);
  assert(std::abs(static_cast<double>(w) - samples * warm) < 1.1);
}
#endif
#endif
}  // namespace

int main() {
  auto& light = Frontlight;
  light.setRampMs(0);
#if !FREEINK_DEVICE_X4PRO
  light.begin(5, 50, true);
  assert(!light.present() && !fakeTimer.active && pwmWrites == 0);
  light.setOn(false);
  light.prepareForDeepSleep();
  assert(pwmWrites == 0);
  std::puts("C3/no frontlight: no PWM writes or timer created");
#elif !FREEINK_FRONTLIGHT_DITHER
  light.begin(5, 50, true);
  const auto total = FrontlightManager::percentDuty(5, 2047);
  assert(!fakeTimer.active && pwm[1] == (total * 50 + 50) / 100 && pwm[0] + pwm[1] == total);
  light.setOn(false);
  assert(pwm[0] == 0 && pwm[1] == 0);
  light.setRampMs(200);
  light.setOn(true);
  for (unsigned i = 0; i < 10; ++i) tick();
  assert(!fakeTimer.active && pwm[0] + pwm[1] == total);
  light.prepareForDeepSleep();
  assert(!fakeTimer.active && pwmDetached[0] && pwmDetached[1]);
  std::puts("Non-dither X4 Pro: original channel rounding, 20ms fade and sleep preserved");
#else
  light.begin(5, 50, true);
  assert(fakeTimer.active && fakeTimer.period == 250);
  mean(4000, 2.5, 2.5);
  neutralTicks(100);

  light.setBrightness(6);
  assert(!fakeTimer.active && pwm[0] == 3 && pwm[1] == 3);
  light.setBrightness(7);
  assert(fakeTimer.active);
  mean(4000, 3.5, 3.5);
  light.setWarmth(0);
  assert(!fakeTimer.active && pwm[0] == 7 && pwm[1] == 0);
  light.setWarmth(100);
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 7);
  light.setWarmth(30);
  assert(fakeTimer.active);
  mean(4000, 4.9, 2.1);

  light.setOn(false);
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);
  const auto writes = pwmWrites;
  fakeTimer.args.callback(fakeTimer.args.arg);  // cancelled callback already queued before off
  assert(pwmWrites == writes);
  light.setBrightness(5);
  light.setWarmth(50);
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);

  light.setRampMs(200);
  light.setOn(true);
  neutralTicks(800);  // fade completes, fractional steady mix must continue
  assert(fakeTimer.active);
  mean(4000, 2.5, 2.5);
  light.setOn(false);
  neutralTicks(200);
  const auto beforeReverse = pwm[0];
  light.setOn(true);
  tick();
  assert(pwm[0] == pwm[1] && pwm[0] <= beforeReverse + 1);
  neutralTicks(800);

  light.rampOutputTo(0);
  for (unsigned i = 0; i < 800 && fakeTimer.active; ++i) {
    tick();
    assert(pwm[0] == pwm[1]);
  }
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);
  light.restoreOutput(true);
  neutralTicks(800);
  assert(fakeTimer.active);
  light.prepareForDeepSleep();
  assert(!fakeTimer.active && pwmDetached[0] && pwmDetached[1]);
  const auto parkedWrites = pwmWrites;
  fakeTimer.args.callback(fakeTimer.args.arg);
  assert(pwmWrites == parkedWrites && pwm[0] == 0 && pwm[1] == 0);

  light.begin(5, 50, true);
  neutralTicks(800);
  light.setRampMs(0);
  light.setWarmth(0);
  light.setBrightness(100);
  assert(!fakeTimer.active && pwm[0] == 2047 && pwm[1] == 0);
  light.setWarmth(100);
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 2047);

  light.setBrightness(5);
  light.setWarmth(50);
  light.setRampMs(200);
  failTimerStart = true;
  light.setOn(false);
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);
  light.setOn(true);
  assert(light.isOn() && !fakeTimer.active && pwm[0] == pwm[1]);
  failTimerStart = false;
  light.setBrightness(5);
  assert(fakeTimer.active);
  mean(4000, 2.5, 2.5);
  light.setPreciseTemperature(false);
  assert(!fakeTimer.active && pwm[0] == 2 && pwm[1] == 3);
  const auto disabledWrites = pwmWrites;
  fakeTimer.args.callback(fakeTimer.args.arg);
  assert(pwmWrites == disabledWrites);
  for (unsigned brightness = 1; brightness <= 100; ++brightness) {
    light.setBrightness(brightness);
    for (unsigned warmth = 0; warmth <= 100; ++warmth) {
      light.setWarmth(warmth);
      assert(!fakeTimer.active);
    }
  }
  light.setBrightness(5);
  light.setWarmth(50);
  light.setPreciseTemperature(true);
  assert(fakeTimer.active);
  mean(4000, 2.5, 2.5);
  light.setBrightness(10);
  light.setWarmth(35);
  assert(fakeTimer.active);
  mean(4000, 6.5, 3.5);
  light.setBrightness(11);
  assert(!fakeTimer.active && pwm[0] == 7 && pwm[1] == 4);
  light.setBrightness(10);
  assert(fakeTimer.active);
  light.setBrightness(100);
  light.setWarmth(50);
  assert(!fakeTimer.active && pwm[0] == 1023 && pwm[1] == 1024);

  // Sweep both sides of the 10% policy boundary over every warmth.
  light.setRampMs(0);
  for (unsigned brightness = 1; brightness <= 100; ++brightness) {
    light.setBrightness(brightness);
    const auto total = FrontlightManager::percentDuty(brightness, 2047);
    for (unsigned warmth = 0; warmth <= 100; ++warmth) {
      light.setWarmth(warmth);
      const bool precise = brightness <= 10;
      assert(fakeTimer.active == (precise && total * warmth % 100 != 0));
      uint64_t c = 0, w = 0;
      for (unsigned i = 0; i < 400; ++i) {
        if (fakeTimer.active) tick();
        assert(pwm[0] <= 2047 && pwm[1] <= 2047);
        if (precise && warmth == 50) assert(pwm[0] == pwm[1]);
        if (warmth == 0) assert(pwm[1] == 0);
        if (warmth == 100) assert(pwm[0] == 0);
        c += pwm[0];
        w += pwm[1];
      }
      if (precise) {
        assert(std::abs(static_cast<double>(c) - 400.0 * total * (100 - warmth) / 100) < 1.1);
        assert(std::abs(static_cast<double>(w) - 400.0 * total * warmth / 100) < 1.1);
      } else {
        assert(pwm[0] + pwm[1] == total);
        assert(std::abs(static_cast<double>(pwm[1]) - total * warmth / 100.0) <= 0.5);
      }
    }
  }
  light.setBrightness(100);
  light.setWarmth(50);
  light.setRampMs(200);
  light.setOn(false);
  bool sawLowMix = false;
  for (unsigned i = 0; i < 900 && fakeTimer.active; ++i) {
    tick();
    if (pwm[0] + pwm[1] <= 10 && pwm[0] + pwm[1] > 0) {
      assert(pwm[0] == pwm[1]);
      sawLowMix = true;
    }
  }
  assert(sawLowMix && !fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);
  light.setOn(true);
  for (unsigned i = 0; i < 900 && fakeTimer.active; ++i) tick();
  assert(!fakeTimer.active && pwm[0] == 1023 && pwm[1] == 1024);
  light.setOn(false);
  light.setPreciseTemperature(false);  // changing the setting must not cancel a fade
  for (unsigned i = 0; i < 900 && fakeTimer.active; ++i) tick();
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);
  light.setRampMs(0);
  light.setOn(false);
  assert(!fakeTimer.active && pwm[0] == 0 && pwm[1] == 0);
  std::puts(
      "Real HAL+SDK: runtime option, 10/11% gate, 100/50 idle, low mix, fade/reversal, off/sleep/queued callback and "
      "timer failure "
      "passed");
#endif
}
