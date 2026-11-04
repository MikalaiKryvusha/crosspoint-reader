#include "PanelCurve.h"

#include <HalStorage.h>
#include <Logging.h>

#include <string>

namespace panelcurve {
namespace {
// Inverse of this X4 Pro's measured sleep-screen response (tools/sleep_image.py PANEL_RESPONSE, 03.11.2026).
constexpr uint8_t BUILT_IN[256] = {
    0, 2, 3, 5, 7, 8, 10, 12, 13, 14, 14, 15, 15, 16, 17, 17,
    18, 19, 19, 20, 20, 21, 22, 22, 23, 24, 24, 25, 25, 26, 26, 26,
    26, 26, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 29, 29, 29,
    29, 29, 29, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 32, 32,
    32, 32, 32, 32, 33, 33, 33, 33, 33, 34, 34, 34, 34, 34, 34, 35,
    35, 35, 35, 35, 35, 36, 36, 36, 36, 36, 37, 37, 37, 37, 37, 37,
    38, 38, 38, 38, 39, 40, 40, 41, 42, 43, 44, 45, 45, 46, 47, 48,
    49, 50, 50, 51, 52, 52, 53, 53, 54, 54, 55, 56, 56, 57, 57, 58,
    58, 59, 59, 60, 61, 61, 62, 62, 63, 63, 64, 64, 65, 65, 65, 66,
    66, 67, 67, 67, 68, 68, 68, 69, 69, 70, 70, 70, 71, 71, 72, 72,
    72, 73, 73, 73, 74, 74, 75, 75, 75, 76, 76, 77, 77, 77, 78, 78,
    78, 79, 79, 80, 80, 80, 81, 81, 82, 82, 82, 83, 83, 83, 84, 84,
    85, 85, 85, 86, 86, 87, 87, 87, 88, 88, 88, 89, 89, 93, 98, 102,
    106, 110, 114, 118, 123, 127, 131, 135, 139, 143, 148, 152, 154, 155, 156, 157,
    158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 171, 172, 173, 174,
    176, 177, 178, 180, 181, 183, 185, 186, 188, 190, 192, 195, 198, 201, 204, 217,
};

uint32_t fnv1a(const uint8_t* data) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 256; ++i) h = (h ^ data[i]) * 16777619u;
  return h;
}

bool useBuiltIn(uint8_t table[256], uint32_t& hash) {
  for (int i = 0; i < 256; ++i) table[i] = BUILT_IN[i];
  hash = fnv1a(BUILT_IN);
  LOG_DBG("CRV", "Built-in panel curve: %08x", static_cast<unsigned>(hash));
  return true;
}
}  // namespace

bool load(uint8_t table[256], uint32_t& hash) {
  if (!Storage.exists(PATH)) return useBuiltIn(table, hash);
  std::string text;
  if (!Storage.readFileToString("CRV", PATH, 4096, text)) return useBuiltIn(table, hash);

  uint8_t parsed[256];
  int count = 0;
  int value = -1;
  for (const char c : text) {
    if (c >= '0' && c <= '9') {
      value = (value < 0 ? 0 : value * 10) + (c - '0');
      if (value > 255) {
        LOG_ERR("CRV", "Value above 255 in %s", PATH);
        return useBuiltIn(table, hash);
      }
    } else if (value >= 0) {
      if (count == 256) {
        LOG_ERR("CRV", "More than 256 values in %s", PATH);
        return useBuiltIn(table, hash);
      }
      parsed[count++] = static_cast<uint8_t>(value);
      value = -1;
    }
  }
  if (value >= 0 && count < 256) parsed[count++] = static_cast<uint8_t>(value);
  if (count != 256) {
    LOG_ERR("CRV", "%s has %d values, expected 256", PATH, count);
    return useBuiltIn(table, hash);
  }

  for (int i = 0; i < 256; ++i) table[i] = parsed[i];
  const uint32_t h = fnv1a(parsed);
  hash = h;
  LOG_DBG("CRV", "Panel curve loaded: %08x", static_cast<unsigned>(h));
  return true;
}

}  // namespace panelcurve
