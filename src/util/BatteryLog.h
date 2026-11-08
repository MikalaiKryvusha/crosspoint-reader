#pragma once

// Debug build only (KRINIK_BATTERY_LOG): appends battery telemetry to /.crosspoint/battery_log.csv —
// every 20 minutes while awake, on charging changes (at most every 10 seconds), at boot and before sleep.
// The data answers
// which gauge voltages the 0% and 100% readings sit at and how much the frontlight draws.
namespace batterylog {
// event: "boot", "tick", "charge", "sleep". Periodic "tick" is every 20 minutes; explicit events reset its timer.
void record(const char* event);
// Call from the main loop: writes a "tick" every 20 minutes and throttled "charge" events.
void tick();
}  // namespace batterylog
