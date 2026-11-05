#include "PanelCurve.h"

#include <HalStorage.h>
#include <Logging.h>

#include <string>

#include "CrossPointSettings.h"

namespace panelcurve {
namespace {
// Inverse of this X4 Pro's measured sleep-screen response (tools/sleep_image.py PANEL_RESPONSE, 03.11.2026).
constexpr uint8_t BUILT_IN[256] = {
    0,   2,   3,   5,   7,   8,   10,  12,  13,  14,  14,  15,  15,  16,  17,  17,  18,  19,  19,  20,  20,  21,
    22,  22,  23,  24,  24,  25,  25,  26,  26,  26,  26,  26,  27,  27,  27,  27,  27,  27,  28,  28,  28,  28,
    28,  29,  29,  29,  29,  29,  29,  30,  30,  30,  30,  30,  30,  31,  31,  31,  31,  31,  32,  32,  32,  32,
    32,  32,  33,  33,  33,  33,  33,  34,  34,  34,  34,  34,  34,  35,  35,  35,  35,  35,  35,  36,  36,  36,
    36,  36,  37,  37,  37,  37,  37,  37,  38,  38,  38,  38,  39,  40,  40,  41,  42,  43,  44,  45,  45,  46,
    47,  48,  49,  50,  50,  51,  52,  52,  53,  53,  54,  54,  55,  56,  56,  57,  57,  58,  58,  59,  59,  60,
    61,  61,  62,  62,  63,  63,  64,  64,  65,  65,  65,  66,  66,  67,  67,  67,  68,  68,  68,  69,  69,  70,
    70,  70,  71,  71,  72,  72,  72,  73,  73,  73,  74,  74,  75,  75,  75,  76,  76,  77,  77,  77,  78,  78,
    78,  79,  79,  80,  80,  80,  81,  81,  82,  82,  82,  83,  83,  83,  84,  84,  85,  85,  85,  86,  86,  87,
    87,  87,  88,  88,  88,  89,  89,  93,  98,  102, 106, 110, 114, 118, 123, 127, 131, 135, 139, 143, 148, 152,
    154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 171, 172, 173, 174, 176, 177,
    178, 180, 181, 183, 185, 186, 188, 190, 192, 195, 198, 201, 204, 217,
};

uint32_t fnv1a(const uint8_t* data) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 256; ++i) h = (h ^ data[i]) * 16777619u;
  return h;
}

// Inverse of this X4 Pro's measured response for images inside books (tools/sleep_image.py BOOK_RESPONSE, 03.11.2026;
// the same table as `sleep_image.py --book`).
constexpr uint8_t BOOK_BUILT_IN[256] = {
    0,   8,   16,  24,  32,  38,  39,  40,  41,  41,  42,  43,  44,  45,  45,  46,  47,  48,  49,  49,  50,  51,
    52,  52,  53,  54,  54,  55,  56,  56,  57,  58,  59,  59,  60,  61,  61,  62,  63,  63,  64,  65,  66,  67,
    68,  69,  69,  70,  71,  72,  73,  74,  75,  76,  76,  77,  78,  78,  79,  80,  81,  81,  82,  83,  83,  84,
    85,  85,  86,  87,  87,  88,  89,  89,  90,  91,  91,  92,  92,  93,  94,  94,  95,  96,  96,  97,  98,  98,
    99,  100, 100, 101, 101, 102, 103, 104, 105, 106, 106, 107, 108, 109, 110, 111, 111, 112, 113, 114, 115, 115,
    116, 117, 117, 118, 119, 119, 120, 121, 121, 122, 123, 123, 124, 125, 125, 126, 127, 127, 129, 133, 136, 140,
    141, 141, 142, 142, 142, 143, 143, 144, 144, 145, 145, 145, 146, 146, 147, 147, 148, 148, 149, 149, 149, 150,
    150, 151, 151, 152, 152, 152, 153, 153, 154, 154, 155, 155, 155, 156, 156, 157, 157, 158, 158, 158, 159, 159,
    160, 160, 161, 161, 161, 162, 162, 163, 163, 163, 164, 164, 165, 165, 166, 166, 167, 167, 168, 168, 169, 170,
    170, 171, 171, 172, 173, 173, 174, 174, 175, 175, 176, 177, 177, 178, 178, 180, 181, 183, 184, 186, 187, 189,
    190, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 206, 207, 208, 209, 210, 211, 212, 214,
    215, 216, 217, 219, 221, 222, 224, 226, 228, 230, 233, 236, 239, 242};

constexpr uint8_t IDENTITY[256] = {
#define R16(b) \
  b, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7, b + 8, b + 9, b + 10, b + 11, b + 12, b + 13, b + 14, b + 15
    R16(0),   R16(16),  R16(32),  R16(48),  R16(64),  R16(80),  R16(96),  R16(112),
    R16(128), R16(144), R16(160), R16(176), R16(192), R16(208), R16(224), R16(240),
#undef R16
};

struct Source {
  const char* rootPath;
  const char* hiddenPath;
  const uint8_t* builtIn;
};
constexpr Source PICTURES{ROOT_PATH, PATH, BUILT_IN};
constexpr Source BOOKS{BOOK_ROOT_PATH, BOOK_PATH, BOOK_BUILT_IN};

void useBuiltIn(const Source& src, uint8_t table[256], uint32_t& hash) {
  for (int i = 0; i < 256; ++i) table[i] = src.builtIn[i];
  hash = fnv1a(src.builtIn);
  LOG_DBG("CRV", "Built-in curve %s: %08x", src.rootPath, static_cast<unsigned>(hash));
}

// Reads the card file (root first, then the hidden folder) or falls back to the built-in curve.
void readFromCard(const Source& src, uint8_t table[256], uint32_t& hash) {
  const char* path = Storage.exists(src.rootPath) ? src.rootPath : src.hiddenPath;
  std::string text;
  if (!Storage.exists(path) || !Storage.readFileToString("CRV", path, 4096, text)) return useBuiltIn(src, table, hash);

  uint8_t parsed[256];
  int count = 0;
  int value = -1;
  for (const char c : text) {
    if (c >= '0' && c <= '9') {
      value = (value < 0 ? 0 : value * 10) + (c - '0');
      if (value > 255) {
        LOG_ERR("CRV", "Value above 255 in %s", path);
        return useBuiltIn(src, table, hash);
      }
    } else if (value >= 0) {
      if (count == 256) {
        LOG_ERR("CRV", "More than 256 values in %s", path);
        return useBuiltIn(src, table, hash);
      }
      parsed[count++] = static_cast<uint8_t>(value);
      value = -1;
    }
  }
  if (value >= 0 && count < 256) parsed[count++] = static_cast<uint8_t>(value);
  if (count != 256) {
    LOG_ERR("CRV", "%s has %d values, expected 256", path, count);
    return useBuiltIn(src, table, hash);
  }
  for (int i = 0; i < 256; ++i) table[i] = parsed[i];
  hash = fnv1a(parsed);
  LOG_DBG("CRV", "Curve loaded from %s: %08x", path, static_cast<unsigned>(hash));
}

// Each curve is 256 bytes: read from the card once after boot, then kept in RAM. The checkboxes are read on every call,
// so switching one takes effect at once.
struct Cached {
  uint8_t table[256];
  uint32_t hash = 0;
  bool ready = false;
};
const Cached& cached(const Source& src, Cached& slot) {
  if (!slot.ready) {
    readFromCard(src, slot.table, slot.hash);
    slot.ready = true;
  }
  return slot;
}
Cached picturesSlot;
Cached booksSlot;
}  // namespace

bool load(uint8_t table[256], uint32_t& hash) {
  if (!SETTINGS.panelCalPictures) {
    for (int i = 0; i < 256; ++i) table[i] = IDENTITY[i];
    hash = fnv1a(IDENTITY);
    return true;
  }
  const Cached& c = cached(PICTURES, picturesSlot);
  for (int i = 0; i < 256; ++i) table[i] = c.table[i];
  hash = c.hash;
  return true;
}

const uint8_t* bookCurve(uint32_t& hash) {
  if (!SETTINGS.panelCalBooks) {
    hash = 0;
    return nullptr;
  }
  const Cached& c = cached(BOOKS, booksSlot);
  hash = c.hash;
  return c.table;
}

}  // namespace panelcurve
