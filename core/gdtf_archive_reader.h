#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace gdtf {

enum class ArchiveDiagnosticCode {
  None,
  EmptySourcePath,
  OpenFailed,
  NoReadableEntries,
  MissingDescriptionXml,
  DuplicateDescriptionXml,
  AmbiguousDescriptionXml,
  NonCanonicalDescriptionXml,
  EmptyDescriptionXml,
  UnsafeEntryPath,
  EntryReadFailed,
  EntryTooLarge,
  FilesystemError,
  UnexpectedException,
  Utf8FlagMissing,
  Utf8FallbackUsed,
  LegacyFilenameEncodingUsed,
  FilenameDecodeFailed,
  FilenameEncodingAmbiguous
};

struct ArchiveDiagnostic {
  ArchiveDiagnosticCode code = ArchiveDiagnosticCode::None;
  std::string message;
  std::string entryPath;
};

struct ArchiveEntry {
  std::string path;
  std::uint64_t size = 0;
  bool sizeKnown = false;
  bool directory = false;
  bool nameUsedUtf8CompatibilityFallback = false;
};

struct ArchiveReadResult {
  std::filesystem::path sourcePath;
  std::string descriptionXml;
  std::string descriptionEntryPath;
  std::vector<ArchiveEntry> entries;
  std::vector<ArchiveDiagnostic> diagnostics;
  bool usedCompatibilityDescriptionFallback = false;
  bool standardsCompliantDescriptionLocation = false;
  std::size_t utf8FlagMissingEntryCount = 0;

  bool Success() const;
};

ArchiveReadResult ReadGdtfArchive(const std::filesystem::path &sourcePath);

ArchiveReadResult
ExtractGdtfArchive(const std::filesystem::path &sourcePath,
                   const std::filesystem::path &destinationRoot);

} // namespace gdtf
