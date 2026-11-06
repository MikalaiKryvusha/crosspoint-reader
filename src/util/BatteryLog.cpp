#include "BatteryLog.h"

#if KRINIK_BATTERY_LOG
#include <Arduino.h>
#ifndef SIMULATOR
#include <BatteryMonitor.h>
#endif
#include <HalFrontlight.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <ctime>

#include "activities/RenderLock.h"

namespace batterylog {
namespace {
constexpr const char* LOG_PATH = "/.crosspoint/battery_log.csv";
constexpr unsigned long TICK_MS = 60UL * 1000UL;
unsigned long lastTickMs = 0;
int lastCharging = -1;

#ifdef SIMULATOR
// The simulator has no gauge: fixed values, so the file format and the hooks can be tested.
struct SimBattery {
  bool readPercentageChecked(uint16_t& out) const {
    out = 100;
    return true;
  }
  uint16_t readMillivolts() const { return 0; }
  bool isCharging() const { return false; }
};
const SimBattery& battery() {
  static const SimBattery monitor;
  return monitor;
}
#else
const BatteryMonitor& battery() {
  static const BatteryMonitor monitor;
  return monitor;
}
#endif

// The gauge shares the I2C bus with touch and the RTC; the themes read it under the render lock.
bool write(const char* event, bool& charging) {
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return false;
  uint16_t percent = 0;
  const bool percentOk = battery().readPercentageChecked(percent);
  const uint16_t millivolts = battery().readMillivolts();
  charging = battery().isCharging();

  const bool isNew = !Storage.exists(LOG_PATH);
  HalFile file = Storage.open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("BATLOG", "cannot open %s", LOG_PATH);
    return true;  // nothing to retry: no card or a full card
  }
  char line[128];
  if (isNew) {
    file.print("epoch,uptime_s,event,vcell_mv,soc_pct,charging,light_on,brightness_pct,warm_pct\n");
  }
  snprintf(line, sizeof(line), "%lld,%lu,%s,%u,%d,%d,%d,%u,%u\n", static_cast<long long>(time(nullptr)),
           millis() / 1000UL, event, millivolts, percentOk ? static_cast<int>(percent) : -1, charging ? 1 : 0,
           Frontlight.isOn() ? 1 : 0, Frontlight.brightness(), Frontlight.warmth());
  file.print(line);
  file.close();
  return true;
}
}  // namespace

void record(const char* event) {
  bool charging = false;
  if (write(event, charging)) {
    lastCharging = charging ? 1 : 0;
    lastTickMs = millis();
  }
}

void tick() {
  const unsigned long now = millis();
  const bool due = lastTickMs == 0 || now - lastTickMs >= TICK_MS;
  // The charge pin is a plain GPIO read: cheap to check every pass.
  const int charging = battery().isCharging() ? 1 : 0;
  if (due) {
    record("tick");
  } else if (lastCharging >= 0 && charging != lastCharging) {
    record("charge");
  }
}
}  // namespace batterylog

#else
namespace batterylog {
void record(const char*) {}
void tick() {}
}  // namespace batterylog
#endif
