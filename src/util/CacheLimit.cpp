#include "CacheLimit.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace cachelimit {
namespace {
constexpr const char* CACHE_DIR = "/.crosspoint";
// Deletions per call: a card from before the limit can hold hundreds of cache files; rotate them out gradually
// instead of blocking one paint for seconds.
constexpr size_t MAX_DELETIONS_PER_CALL = 16;

bool hasSuffix(const char* name, const char* suffix) {
  const size_t len = strlen(name);
  const size_t suffixLen = strlen(suffix);
  return len >= suffixLen && strcmp(name + len - suffixLen, suffix) == 0;
}

bool isCacheName(const char* name, const char* prefix) {
  return strncmp(name, prefix, strlen(prefix)) == 0 && hasSuffix(name, ".bmp");
}

// Queue lines are trusted only when they name a cache file of this kind in /.crosspoint: a damaged queue file must
// never make the rotation delete anything else on the card (review 04.11.2026).
bool isCachePath(const std::string& path, const char* prefix) {
  const std::string dir = std::string(CACHE_DIR) + "/";
  if (path.compare(0, dir.size(), dir) != 0) return false;
  const std::string name = path.substr(dir.size());
  return name.find('/') == std::string::npos && isCacheName(name.c_str(), prefix);
}

std::vector<std::string> readQueue(const std::string& queuePath) {
  std::vector<std::string> paths;
  HalFile file;
  if (!Storage.openFileForRead("CACHE", queuePath, file)) return paths;
  std::string line;
  char buffer[128];
  int read;
  while ((read = file.read(buffer, sizeof(buffer))) > 0) {
    for (int i = 0; i < read; ++i) {
      if (buffer[i] == '\n') {
        if (!line.empty()) paths.push_back(line);
        line.clear();
      } else if (line.size() < 160) {
        line += buffer[i];
      }
    }
  }
  if (!line.empty()) paths.push_back(line);
  file.close();
  return paths;
}

void writeQueue(const std::string& queuePath, const std::vector<std::string>& paths) {
  const std::string tmp = queuePath + ".tmp";
  HalFile file;
  if (!Storage.openFileForWrite("CACHE", tmp, file)) return;
  bool ok = true;
  for (const auto& path : paths) {
    ok = ok && file.write(reinterpret_cast<const uint8_t*>(path.data()), path.size()) == path.size();
    ok = ok && file.write(reinterpret_cast<const uint8_t*>("\n"), 1) == 1;
  }
  ok = file.close() && ok;
  if (ok) {
    Storage.replaceFile(tmp.c_str(), queuePath.c_str());
  } else {
    Storage.remove(tmp.c_str());
  }
}
}  // namespace

void remember(const char* prefix, const char* justWrittenPath, const size_t keep) {
  const std::string queuePath = std::string(CACHE_DIR) + "/" + prefix + "cache.idx";
  const std::string justWritten = justWrittenPath;

  // One pass over /.crosspoint: the cache files of this kind present now (directory order), and leftover
  // <prefix>*.bmp.tmp files from an interrupted conversion, which are removed.
  std::vector<std::string> present;
  std::vector<std::string> leftovers;
  HalFile dir = Storage.open(CACHE_DIR);
  if (dir && dir.isDirectory()) {
    char name[96];
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) continue;
      file.getName(name, sizeof(name));
      if (strncmp(name, prefix, strlen(prefix)) != 0) continue;
      std::string path = std::string(CACHE_DIR) + "/" + name;
      if (hasSuffix(name, ".bmp.tmp")) {
        if (path != justWritten + ".tmp") leftovers.push_back(std::move(path));
      } else if (isCacheName(name, prefix) && path != justWritten) {
        present.push_back(std::move(path));
      }
    }
    dir.close();
  }
  for (const auto& path : leftovers) Storage.remove(path.c_str());

  // The queue (oldest first) keeps only valid lines whose file is present; present files it does not know are the
  // oldest of all, in directory order. The file just written goes last.
  const std::unordered_set<std::string> presentSet(present.begin(), present.end());
  std::unordered_set<std::string> queued;
  std::vector<std::string> known;
  for (auto& path : readQueue(queuePath)) {
    if (!isCachePath(path, prefix) || !presentSet.count(path) || !queued.insert(path).second) continue;
    known.push_back(std::move(path));
  }
  std::vector<std::string> queue;
  queue.reserve(present.size() + 1);
  for (auto& path : present) {
    if (!queued.count(path)) queue.push_back(std::move(path));
  }
  queue.insert(queue.end(), std::make_move_iterator(known.begin()), std::make_move_iterator(known.end()));
  queue.push_back(justWritten);

  size_t dropped = 0;
  // queue.size() - dropped is what stays; the last entry (the file just written) is never removed.
  while (queue.size() - dropped > keep && queue.size() - dropped > 1 && dropped < MAX_DELETIONS_PER_CALL) {
    Storage.remove(queue[dropped].c_str());
    ++dropped;
  }
  queue.erase(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(dropped));
  writeQueue(queuePath, queue);
  if (dropped > 0 || !leftovers.empty()) {
    LOG_DBG("CACHE", "Rotated out %u oldest %s* cache files, %u leftovers (limit %u)", static_cast<unsigned>(dropped),
            prefix, static_cast<unsigned>(leftovers.size()), static_cast<unsigned>(keep));
  }
}
}  // namespace cachelimit
