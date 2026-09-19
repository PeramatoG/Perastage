#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace perastage::archive::zip {

enum class DirectoryReadStatus : std::uint8_t {
  Success,
  OpenFailed,
  Malformed,
  MultiDiskUnsupported,
  Zip64Unsupported,
};

// Preserves raw central-directory filename metadata in archive order.
struct DirectoryEntry {
  std::string bytes;
  bool utf8Flag = false;
  std::uint64_t uncompressedSize = 0;
  bool directory = false;
};

// Carries bounded classic-ZIP directory metadata or a structural failure.
struct DirectoryReadResult {
  DirectoryReadStatus status = DirectoryReadStatus::Malformed;
  std::vector<DirectoryEntry> entries;

  bool Success() const;
};

DirectoryReadResult ReadDirectory(const std::filesystem::path &archivePath);

bool IsValidUtf8(const std::string &text);

} // namespace perastage::archive::zip
