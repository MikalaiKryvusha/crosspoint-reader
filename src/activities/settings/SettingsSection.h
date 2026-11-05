#pragma once

#include <I18n.h>

#include <cstddef>
#include <cstdint>

// Top-level groups on the Settings screen, in display order. End doubles as
// "no section" where one is optional.
// ScreenClean and ScreenCalibration sit past End: not top-level groups, they are sub-screens of Display («Настройки
// очисток экрана», «Калибровка экрана», Nikolai 05.11.2026) and never index SETTINGS_SECTION_TITLES.
enum class SettingsSection : uint8_t {
  General,
  Display,
  Reader,
  Controls,
  Library,
  Network,
  End,
  ScreenClean,
  ScreenCalibration
};

// A sub-screen of Display to reopen when Settings opens at Display (the calibration screens return there, Nikolai
// 05.11.2026); End — none. Read and cleared by SettingsActivity.
inline SettingsSection pendingSettingsSubSection = SettingsSection::End;

inline constexpr size_t SETTINGS_SECTION_COUNT = static_cast<size_t>(SettingsSection::End);

inline constexpr StrId SETTINGS_SECTION_TITLES[SETTINGS_SECTION_COUNT] = {
    StrId::STR_SETTINGS_GENERAL, StrId::STR_CAT_DISPLAY, StrId::STR_CAT_READER,
    StrId::STR_CAT_CONTROLS,     StrId::STR_LIBRARY,     StrId::STR_SETTINGS_NETWORK,
};
