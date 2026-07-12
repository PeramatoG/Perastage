#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

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
  FilenameEncodingAmbiguous,
  ResourceNotFound,
  ResourcePathAmbiguous,
  ResourceEntryTooLarge,
  ResourceReadFailed,
  UnsafeResourcePath,
  ResourceFilenameDecodeFailed
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

struct GdtfResourceReadResult {
  std::filesystem::path sourcePath;
  std::string requestedPath;
  std::string entryPath;
  std::vector<unsigned char> bytes;
  std::uint64_t size = 0;
  std::string mediaKind;
  std::vector<ArchiveDiagnostic> diagnostics;
  bool caseInsensitiveFallback = false;
  bool filesystemFallback = false;
  bool Success() const;
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

GdtfResourceReadResult ReadGdtfArchiveResource(const std::filesystem::path &sourcePath,
                                               const std::string &requestedPath,
                                               std::uint64_t maxBytes = 4ull * 1024ull * 1024ull);

ArchiveReadResult
ExtractGdtfArchive(const std::filesystem::path &sourcePath,
                   const std::filesystem::path &destinationRoot);

} // namespace gdtf
