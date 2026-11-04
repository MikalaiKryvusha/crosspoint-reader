#include "PanelCurve.h"

#include <HalStorage.h>
#include <Logging.h>

#include <string>

namespace panelcurve {

bool load(uint8_t table[256], uint32_t& hash) {
  if (!Storage.exists(PATH)) return false;
  std::string text;
  if (!Storage.readFileToString("CRV", PATH, 4096, text)) return false;

  uint8_t parsed[256];
  int count = 0;
  int value = -1;
  for (const char c : text) {
    if (c >= '0' && c <= '9') {
      value = (value < 0 ? 0 : value * 10) + (c - '0');
      if (value > 255) {
        LOG_ERR("CRV", "Value above 255 in %s", PATH);
        return false;
      }
    } else if (value >= 0) {
      if (count == 256) {
        LOG_ERR("CRV", "More than 256 values in %s", PATH);
        return false;
      }
      parsed[count++] = static_cast<uint8_t>(value);
      value = -1;
    }
  }
  if (value >= 0 && count < 256) parsed[count++] = static_cast<uint8_t>(value);
  if (count != 256) {
    LOG_ERR("CRV", "%s has %d values, expected 256", PATH, count);
    return false;
  }

  uint32_t h = 2166136261u;  // FNV-1a
  for (int i = 0; i < 256; ++i) {
    table[i] = parsed[i];
    h = (h ^ parsed[i]) * 16777619u;
  }
  hash = h;
  return true;
}

}  // namespace panelcurve
