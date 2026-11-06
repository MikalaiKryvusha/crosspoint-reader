#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_TIMER_TASK = 0;
struct esp_timer_create_args_t {
  void (*callback)(void*);
  void* arg;
  int dispatch_method;
  const char* name;
};
struct FakeTimer {
  esp_timer_create_args_t args{};
  bool active = false;
  uint64_t period = 0;
};
using esp_timer_handle_t = FakeTimer*;
inline FakeTimer fakeTimer;
inline int64_t fakeTimeUs = 0;
inline bool failTimerStart = false;
inline int esp_timer_create(const esp_timer_create_args_t* a, esp_timer_handle_t* out) {
  fakeTimer.args = *a;
  *out = &fakeTimer;
  return ESP_OK;
}
inline int esp_timer_start_periodic(esp_timer_handle_t t, uint64_t us) {
  if (failTimerStart) return ESP_FAIL;
  t->period = us;
  t->active = true;
  return ESP_OK;
}
inline int esp_timer_stop(esp_timer_handle_t t) {
  if (!t) return ESP_FAIL;
  t->active = false;
  return ESP_OK;
}
inline int64_t esp_timer_get_time() { return fakeTimeUs; }
