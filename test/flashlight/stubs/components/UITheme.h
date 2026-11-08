#pragma once
#include <GfxRenderer.h>

#include <string>
struct Rect {
  int x, y, width, height;
};
struct Metrics {
  int topPadding = 5, headerHeight = 84;
};
struct UITheme {
  static UITheme& getInstance() {
    static UITheme t;
    return t;
  }
  const Metrics& getMetrics() const {
    static Metrics m;
    return m;
  }
};
inline Rect lastHeader{};
inline std::string lastLabel;
struct Gui {
  void drawHeader(GfxRenderer&, Rect r, const char* label) {
    lastHeader = r;
    lastLabel = label;
  }
};
inline Gui GUI;
constexpr int STR_FLASHLIGHT_EXIT = 0;
inline const char* tr(int) { return "Exit"; }
