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
// Nikolai 04.11.2026: «кеш на 100 файлов». A full-screen 2-bit image is ≈ 94 KB, so each cache stays under ≈ 9.4 MB.
constexpr size_t VIEWER_CACHE_FILES = 100;
constexpr size_t SLEEP_CACHE_FILES = 100;

// `prefix` is the cache file name prefix in /.crosspoint ("view_", "sleep_"); `justWrittenPath` the full path of the
// cache file just written.
void remember(const char* prefix, const char* justWrittenPath, size_t keep);

// Deletes every cache file of this kind and its queue. The sleep cache key holds the image's size and FAT date, and
// the firmware writes no dates: a new /sleep.bmp of the same size would reuse the old cache — so «Set as sleep cover»
// drops the sleep caches (review 04.11.2026). They are rebuilt on demand.
void clear(const char* prefix);
}  // namespace cachelimit
