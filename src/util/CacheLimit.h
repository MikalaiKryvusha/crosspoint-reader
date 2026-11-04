#pragma once

#include <cstddef>

// Image caches on the SD card (/.crosspoint/view_*.bmp from the photo viewer, /.crosspoint/sleep_*.bmp from own sleep
// images) had no bound: ≈96 KB per image and orientation, forever (review 04.11.2026; Nikolai: «кеши должны иметь
// лимит», «более старые подчищать, более свежие записывать»). They rotate: after a new cache file is written,
// remember() appends it to a small queue file and deletes the oldest files while there are more than `keep`.
//
// The order lives in the queue file, not in file dates: the firmware sets no FAT date callback, so files it writes
// carry no usable modification time. Cache files found on the card but missing from the queue (older firmware, a
// lost queue) count as the oldest, in directory order.
namespace cachelimit {
constexpr size_t VIEWER_CACHE_FILES = 40;  // ≈ 4 MB
constexpr size_t SLEEP_CACHE_FILES = 30;   // ≈ 3 MB

// `prefix` is the cache file name prefix in /.crosspoint ("view_", "sleep_"); `justWrittenPath` the full path of the
// cache file just written.
void remember(const char* prefix, const char* justWrittenPath, size_t keep);
}  // namespace cachelimit
