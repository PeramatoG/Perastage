#pragma once

#include "gdtfdictionary.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace DictionarySnapshotService {

struct ExportPathStatus {
  size_t totalEntries = 0;
  size_t foundEntries = 0;
  size_t missingEntries = 0;
  std::vector<std::string> missingFiles;
};

struct ImportPathValidation {
  bool validSnapshot = false;
  size_t checkedEntries = 0;
  size_t foundEntries = 0;
  size_t missingEntries = 0;
  std::vector<std::string> missingExamples;
  std::string error;
};

struct SnapshotWriteResult {
  bool success = false;
  std::string error;
};

ExportPathStatus AnalyzeFixtureExportPaths(
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary);
ExportPathStatus AnalyzeTrussExportPaths(
    const std::unordered_map<std::string, std::string> &dictionary);

ImportPathValidation
ValidateFixtureImportPaths(const std::filesystem::path &snapshotPath,
                           const std::filesystem::path &writableLibraryPath);
ImportPathValidation
ValidateTrussImportPaths(const std::filesystem::path &snapshotPath,
                         const std::filesystem::path &writableLibraryPath);

SnapshotWriteResult WriteFixtureSnapshot(
    const std::filesystem::path &outputPath,
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary);
SnapshotWriteResult WriteTrussSnapshot(
    const std::filesystem::path &outputPath,
    const std::unordered_map<std::string, std::string> &dictionary);

} // namespace DictionarySnapshotService
