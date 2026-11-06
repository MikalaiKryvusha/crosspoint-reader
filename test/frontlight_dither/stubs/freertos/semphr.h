#pragma once
using SemaphoreHandle_t = void*;
inline void* xSemaphoreCreateMutex() {
  static int mutex;
  return &mutex;
}
inline int xSemaphoreTake(void*, int) { return 1; }
inline void xSemaphoreGive(void*) {}
