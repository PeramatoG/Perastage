#pragma once

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
  UnexpectedException
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
};

struct ArchiveReadResult {
  std::filesystem::path sourcePath;
  std::string descriptionXml;
  std::string descriptionEntryPath;
  std::vector<ArchiveEntry> entries;
  std::vector<ArchiveDiagnostic> diagnostics;
  bool usedCompatibilityDescriptionFallback = false;
  bool standardsCompliantDescriptionLocation = false;

  bool Success() const;
};

ArchiveReadResult ReadGdtfArchive(const std::filesystem::path &sourcePath);

} // namespace gdtf
