#include "GrayGlass.h"

#include <HalStorage.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace grayglass {
bool imageOnGlass = false;
bool viewerUnderneath = false;

// The on-card trace is a diagnostic for device sessions (the reader has no serial port): build with
// -DKRINIK_SHADE_TRACE=1 to write /.crosspoint/shade_trace.txt. Off by default — it rewrote a file on the SD card on
// every sheet open, viewer exit and sleep (review 04.11.2026).
void trace(const char* fmt, ...) {
#if !defined(KRINIK_SHADE_TRACE) || !KRINIK_SHADE_TRACE
  (void)fmt;
  return;
#else
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
#endif
}
}  // namespace grayglass
