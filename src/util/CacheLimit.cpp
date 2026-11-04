#include "CacheLimit.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace cachelimit {
namespace {
constexpr const char* CACHE_DIR = "/.crosspoint";

bool isCacheFile(const char* name, const char* prefix) {
  const size_t len = strlen(name);
  const size_t prefixLen = strlen(prefix);
  return len >= prefixLen + 4 && strncmp(name, prefix, prefixLen) == 0 && strcmp(name + len - 4, ".bmp") == 0;
}

std::vector<std::string> readQueue(const std::string& queuePath) {
  std::vector<std::string> paths;
  HalFile file;
  if (!Storage.openFileForRead("CACHE", queuePath, file)) return paths;
  std::string line;
  char buffer[128];
  int read;
  while ((read = file.read(reinterpret_cast<uint8_t*>(buffer), sizeof(buffer))) > 0) {
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

  // The queue, oldest first, without entries whose file is gone and without the file just written.
  std::vector<std::string> queue;
  for (auto& path : readQueue(queuePath)) {
    if (path != justWritten && Storage.exists(path.c_str())) queue.push_back(std::move(path));
  }

  // Cache files on the card the queue does not know: the oldest of all, in directory order.
  std::vector<std::string> unknown;
  HalFile dir = Storage.open(CACHE_DIR);
  if (dir && dir.isDirectory()) {
    char name[96];
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) continue;
      file.getName(name, sizeof(name));
      if (!isCacheFile(name, prefix)) continue;
      std::string path = std::string(CACHE_DIR) + "/" + name;
      if (path == justWritten || std::find(queue.begin(), queue.end(), path) != queue.end()) continue;
      unknown.push_back(std::move(path));
    }
    dir.close();
  }
  queue.insert(queue.begin(), unknown.begin(), unknown.end());
  queue.push_back(justWritten);

  size_t dropped = 0;
  while (queue.size() > keep && queue.size() > 1) {
    Storage.remove(queue.front().c_str());
    queue.erase(queue.begin());
    ++dropped;
  }
  writeQueue(queuePath, queue);
  if (dropped > 0) {
    LOG_DBG("CACHE", "Rotated out %u oldest %s* cache files (limit %u)", static_cast<unsigned>(dropped), prefix,
            static_cast<unsigned>(keep));
  }
}
}  // namespace cachelimit
