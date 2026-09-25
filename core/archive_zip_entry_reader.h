#pragma once

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

// Carries one bounded ZIP payload selected by authoritative directory index.
struct EntryReadResult {
  EntryReadStatus status = EntryReadStatus::ReadFailed;
  std::vector<std::uint8_t> bytes;

  bool Success() const;
};

EntryReadResult ReadEntry(const std::filesystem::path &archivePath,
                          std::size_t entryIndex, std::uint64_t maxBytes);
EntryReadResult ReadEntry(std::span<const std::uint8_t> archiveBytes,
                          std::size_t entryIndex, std::uint64_t maxBytes);

} // namespace perastage::archive::zip
