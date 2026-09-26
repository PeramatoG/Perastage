#pragma once

#include "archive_zip_directory.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace perastage::archive::zip {

enum class EntryReadStatus : std::uint8_t {
  Success,
  OpenFailed,
  EntryMissing,
  EntryTooLarge,
  ReadFailed,
};

// Carries one bounded ZIP payload selected by validated local-record identity.
struct EntryReadResult {
  EntryReadStatus status = EntryReadStatus::ReadFailed;
  std::vector<std::uint8_t> bytes;
  bool complete = false;

  bool Success() const;
};

EntryReadResult ReadEntry(const std::filesystem::path &archivePath,
                          const DirectoryEntry &entry, std::uint64_t maxBytes);
EntryReadResult ReadEntry(std::span<const std::uint8_t> archiveBytes,
                          const DirectoryEntry &entry, std::uint64_t maxBytes);
std::vector<EntryReadResult>
ReadEntryPrefixes(const std::filesystem::path &archivePath,
                  const std::vector<DirectoryEntry> &entries,
                  std::uint64_t maxBytes);
std::vector<EntryReadResult>
ReadEntryPrefixes(std::span<const std::uint8_t> archiveBytes,
                  const std::vector<DirectoryEntry> &entries,
                  std::uint64_t maxBytes);

} // namespace perastage::archive::zip
