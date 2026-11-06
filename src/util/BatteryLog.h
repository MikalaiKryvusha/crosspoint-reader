#pragma once

// Debug build only (KRINIK_BATTERY_LOG): appends battery telemetry to /.crosspoint/battery_log.csv —
// once a minute while awake, on every charging change, at boot and before deep sleep. The data answers
// which gauge voltages the 0% and 100% readings sit at and how much the frontlight draws.
namespace batterylog {
// event: "boot", "tick", "charge", "sleep". Throttled to one "tick" a minute; other events always write.
void record(const char* event);
// Call from the main loop: writes a "tick" a minute and a "charge" line when charging starts or stops.
void tick();
}  // namespace batterylog
