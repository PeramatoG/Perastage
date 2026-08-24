#pragma once

#include "filesystem_path_utils.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace viewer3d::resources {

struct PhysicalAssetRevision {
  std::string physicalPathIdentity;
  std::uintmax_t fileSize = 0;
  std::int64_t modificationTimeNs = 0;
  bool metadataAvailable = false;

  bool operator==(const PhysicalAssetRevision &) const = default;
};

// Reads the cheap physical identity used by runtime and disk asset caches.
inline PhysicalAssetRevision ReadPhysicalAssetRevision(const std::string &path) {
  PhysicalAssetRevision revision;
  revision.physicalPathIdentity = PathUtils::BuildFilesystemIdentityKey(path);
  std::error_code sizeError;
  revision.fileSize =
      std::filesystem::file_size(PathUtils::PathFromUtf8(path), sizeError);
  std::error_code timeError;
  const auto writeTime = std::filesystem::last_write_time(
      PathUtils::PathFromUtf8(path), timeError);
  if (!timeError) {
    revision.modificationTimeNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            writeTime.time_since_epoch())
            .count();
  }
  revision.metadataAvailable = !sizeError && !timeError;
  return revision;
}

} // namespace viewer3d::resources
