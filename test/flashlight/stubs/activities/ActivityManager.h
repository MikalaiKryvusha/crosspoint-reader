#pragma once
enum class HomeMenuItem { NONE, FLASHLIGHT };
struct ActivityManager {
  unsigned homes = 0;
  HomeMenuItem selected = HomeMenuItem::NONE;
  void goHome(HomeMenuItem m) {
    ++homes;
    selected = m;
  }
};
inline ActivityManager activityManager;
