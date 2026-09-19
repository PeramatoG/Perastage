#pragma once

#include "inspection/inspection_contract.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace perastage::inspection {

enum class PackageKind : std::uint8_t {
  Gdtf,
  Mvr,
};

enum class PackageEntryType : std::uint8_t {
  File,
  Directory,
};

// Describes one ZIP entry without exposing archive-framework types.
struct PackageEntry {
  std::string displayPath;
  std::optional<std::string> normalizedPath;
  std::string extension;
  PackageEntryType type = PackageEntryType::File;
  std::uint64_t uncompressedSize = 0;
  bool sizeKnown = false;
  bool pathSafe = false;
};

// Carries the ordered, read-only contents of one supported package.
struct PackageInventory {
  PackageKind kind = PackageKind::Gdtf;
  std::vector<PackageEntry> entries;
  bool canonicalRootDocumentPresent = false;
};

// Combines neutral inspection diagnostics with an optional package inventory.
struct PackageInspectionResult {
  Result inspection;
  std::optional<PackageInventory> inventory;
};

namespace package_diagnostic_codes {
inline constexpr char EmptyInputPath[] = "input.empty_path";
inline constexpr char UnsupportedFileType[] = "input.unsupported_file_type";
inline constexpr char OpenFailed[] = "package.open_failed";
inline constexpr char MalformedArchive[] = "package.malformed_archive";
inline constexpr char UnsafeEntryPath[] = "package.unsafe_entry_path";
inline constexpr char FilenameDecodeFailed[] = "package.filename_decode_failed";
inline constexpr char UnexpectedReadFailure[] =
    "package.unexpected_read_failure";
} // namespace package_diagnostic_codes

PackageInspectionResult InspectPackage(const Request &request);
PackageInspectionResult InspectPackage(const std::filesystem::path &sourcePath);

} // namespace perastage::inspection
