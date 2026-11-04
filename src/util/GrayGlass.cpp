#include "GrayGlass.h"

#include <HalStorage.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace grayglass {
bool imageOnGlass = false;
bool viewerUnderneath = false;

void trace(const char* fmt, ...) {
  static std::string lines[8];
  static int count = 0;
  char line[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  lines[count % 8] = std::to_string(count + 1) + ": " + line + "\n";
  ++count;
  HalFile f;
  if (!Storage.openFileForWrite("TRC", "/.crosspoint/shade_trace.txt", f)) return;
  for (int i = count > 8 ? count - 8 : 0; i < count; ++i) {
    const std::string& l = lines[i % 8];
    f.write(reinterpret_cast<const uint8_t*>(l.data()), l.size());
  }
  f.close();
}
}  // namespace grayglass
