#pragma once

#include "font_resources.h"
#include <map>
#include <mutex>

namespace typography {
class SystemFontCache {
public:
  explicit SystemFontCache(std::vector<SystemFontRecord> records);
  FontResolution Resolve(const FontReference &, bool bold, bool italic) const;
  std::shared_ptr<const FontResource> FindResource(std::string_view id) const;
  std::vector<std::string> Families() const;
  const std::string &Signature() const { return signature_; }

private:
  FontResolution Load(size_t record) const;
  struct Entry {
    FontResolution resolution;
    size_t bytes = 0;
    uint64_t sequence = 0;
  };
  struct FileStamp {
    uintmax_t size = 0;
    std::filesystem::file_time_type time;
    bool valid = false;
    bool operator==(const FileStamp &) const = default;
  };
  static FileStamp Stamp(const std::filesystem::path &path);
  std::vector<SystemFontRecord> records_;
  std::vector<FileStamp> stamps_;
  mutable std::vector<std::mutex> loadLocks_;
  std::map<std::string, std::vector<size_t>, std::less<>> families_, postScriptNames_;
  std::string signature_;
  mutable std::mutex mutex_;
  mutable std::map<size_t, Entry> cache_;
  mutable std::map<std::string, size_t, std::less<>> fingerprints_;
  mutable size_t bytes_ = 0;
  mutable uint64_t sequence_ = 0;
};
}
