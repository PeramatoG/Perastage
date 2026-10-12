#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace CacheService {
struct Result {
  std::uintmax_t bytes = 0;
  std::uintmax_t removedFiles = 0;
  std::vector<std::string> errors;
  bool Succeeded() const { return errors.empty(); }
};

// All operations reject symlink directories and report filesystem failures.
bool EnsureDirectories();
Result GetStatistics();
Result Clear();
// Called only with installed and Perastage-owned library roots, never configured
// external user-library paths. Names and binary headers must both match.
Result CleanupLegacyViewerCaches(const std::filesystem::path &libraryRoot);
std::filesystem::path ViewerCachePath(const std::string &sourcePath,
                                      const std::string &mode, bool gdtf);
bool MarkCatalogMigrationComplete();
bool IsSafeCacheFile(const std::filesystem::path &file);
bool IsSafeDirectory(const std::filesystem::path &directory);
} // namespace CacheService
