#include "dictionary_snapshot_service.h"

#include "dictionary_json_contract.h"
#include "filesystem_path_utils.h"
#include "json.hpp"

#include <algorithm>
#include <fstream>
#include <optional>

namespace DictionarySnapshotService {
namespace {

constexpr size_t kMaxExportMissingExamples = 10;
constexpr size_t kMaxImportMissingExamples = 5;

// Reports whether a non-empty filesystem path exists without throwing.
bool HasExistingPath(const std::filesystem::path &candidate) {
  if (candidate.empty())
    return false;
  std::error_code error;
  return std::filesystem::exists(candidate, error);
}

// Resolves a snapshot reference using the established adjacent asset locations.
std::filesystem::path
ResolveImportRelativePath(const std::filesystem::path &snapshotPath,
                          const std::filesystem::path &referencePath) {
  if (referencePath.is_absolute())
    return referencePath;

  const std::filesystem::path snapshotDirectory = snapshotPath.parent_path();
  const std::filesystem::path directPath = snapshotDirectory / referencePath;
  if (HasExistingPath(directPath))
    return directPath;

  const std::filesystem::path compatibilityPath =
      snapshotDirectory / (snapshotPath.stem().string() + "_assets") /
      referencePath;
  if (HasExistingPath(compatibilityPath))
    return compatibilityPath;
  return directPath;
}

// Loads a JSON document and returns a technical error on failure.
std::optional<nlohmann::json> LoadJson(const std::filesystem::path &path,
                                       std::string &error) {
  std::ifstream input(path);
  if (!input.is_open()) {
    error = "Could not open import file";
    return std::nullopt;
  }

  nlohmann::json root;
  try {
    input >> root;
  } catch (const std::exception &exception) {
    error = std::string("Failed to parse JSON: ") + exception.what();
    return std::nullopt;
  }
  return root;
}

// Adds one missing reference example up to the public summary bound.
void RegisterMissingExample(ImportPathValidation &summary,
                            const std::string &entryName,
                            const std::string &reference) {
  if (summary.missingExamples.size() >= kMaxImportMissingExamples)
    return;
  summary.missingExamples.push_back(entryName + " -> " + reference);
}

// Validates file references for one explicit dictionary snapshot type.
ImportPathValidation
ValidateImportPaths(const std::filesystem::path &snapshotPath,
                    const std::filesystem::path &writableLibraryPath,
                    const std::string &dictionaryType) {
  ImportPathValidation summary;
  auto root = LoadJson(snapshotPath, summary.error);
  if (!root)
    return summary;

  auto entries = DictionaryJsonContract::GetEntriesForType(
      *root, dictionaryType, summary.error);
  if (!entries)
    return summary;
  summary.validSnapshot = true;

  const auto checkEntry = [&](const std::string &entryName,
                              const nlohmann::json &entry) {
    if (!entry.is_object())
      return;
    std::string rawPath;
    if (entry.contains("file") && entry["file"].is_string())
      rawPath = entry["file"].get<std::string>();
    else if (entry.contains("path") && entry["path"].is_string())
      rawPath = entry["path"].get<std::string>();
    if (rawPath.empty())
      return;

    ++summary.checkedEntries;
    const std::filesystem::path referencePath =
        PathUtils::PathFromUtf8(rawPath);
    const std::filesystem::path resolvedPath =
        ResolveImportRelativePath(snapshotPath, referencePath);
    const std::filesystem::path libraryPath =
        writableLibraryPath / referencePath.filename();
    if (HasExistingPath(resolvedPath) || HasExistingPath(libraryPath)) {
      ++summary.foundEntries;
      return;
    }

    ++summary.missingEntries;
    RegisterMissingExample(summary, entryName, rawPath);
  };

  const nlohmann::json &entryCollection = **entries;
  if (entryCollection.is_object()) {
    for (auto it = entryCollection.begin(); it != entryCollection.end(); ++it)
      checkEntry(it.key(), it.value());
  } else {
    for (const auto &entry : entryCollection) {
      if (entry.is_object() && entry.contains("name") &&
          entry["name"].is_string()) {
        checkEntry(entry["name"].get<std::string>(), entry);
      }
    }
  }
  return summary;
}

// Writes a dictionary-contract JSON root with stable indentation.
SnapshotWriteResult WriteSnapshot(const std::filesystem::path &outputPath,
                                  const std::string &dictionaryType,
                                  nlohmann::json entries) {
  std::ofstream output(outputPath);
  if (!output.is_open())
    return {false, "Could not open snapshot output file"};
  output << DictionaryJsonContract::MakeRoot(dictionaryType, std::move(entries))
                .dump(4);
  if (!output.good())
    return {false, "Could not write snapshot output file"};
  return {true, {}};
}

} // namespace

// Analyzes fixture reference availability for the export confirmation UI.
ExportPathStatus AnalyzeFixtureExportPaths(
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary) {
  ExportPathStatus summary;
  summary.totalEntries = dictionary.size();
  for (const auto &[name, entry] : dictionary) {
    if (HasExistingPath(PathUtils::PathFromUtf8(entry.path))) {
      ++summary.foundEntries;
    } else {
      ++summary.missingEntries;
      if (summary.missingFiles.size() < kMaxExportMissingExamples) {
        const std::string fileName =
            PathUtils::PathFromUtf8(entry.path).filename().string();
        summary.missingFiles.push_back(fileName.empty() ? name : fileName);
      }
    }
  }
  return summary;
}

// Analyzes truss reference availability for the export confirmation UI.
ExportPathStatus AnalyzeTrussExportPaths(
    const std::unordered_map<std::string, std::string> &dictionary) {
  ExportPathStatus summary;
  summary.totalEntries = dictionary.size();
  for (const auto &[name, path] : dictionary) {
    if (HasExistingPath(PathUtils::PathFromUtf8(path))) {
      ++summary.foundEntries;
    } else {
      ++summary.missingEntries;
      if (summary.missingFiles.size() < kMaxExportMissingExamples) {
        const std::string fileName =
            PathUtils::PathFromUtf8(path).filename().string();
        summary.missingFiles.push_back(fileName.empty() ? name : fileName);
      }
    }
  }
  return summary;
}

// Validates fixture references without applying an import policy.
ImportPathValidation
ValidateFixtureImportPaths(const std::filesystem::path &snapshotPath,
                           const std::filesystem::path &writableLibraryPath) {
  return ValidateImportPaths(snapshotPath, writableLibraryPath, "fixtures");
}

// Validates truss references without applying an import policy.
ImportPathValidation
ValidateTrussImportPaths(const std::filesystem::path &snapshotPath,
                         const std::filesystem::path &writableLibraryPath) {
  return ValidateImportPaths(snapshotPath, writableLibraryPath, "trusses");
}

// Writes a deterministic reference-only fixture dictionary snapshot.
SnapshotWriteResult WriteFixtureSnapshot(
    const std::filesystem::path &outputPath,
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary) {
  std::vector<std::string> keys;
  keys.reserve(dictionary.size());
  for (const auto &[name, entry] : dictionary)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const auto &entry = dictionary.at(name);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty() &&
        entry.visualColorHex.empty())
      continue;
    nlohmann::json value;
    if (!entry.path.empty()) {
      const std::string fileName =
          PathUtils::PathFromUtf8(entry.path).filename().string();
      if (!fileName.empty())
        value["file"] = fileName;
    }
    if (!entry.mode.empty())
      value["mode"] = entry.mode;
    if (!entry.category.empty())
      value["category"] = entry.category;
    if (!entry.visualColorHex.empty())
      value["visual_color"] = entry.visualColorHex;
    if (!value.empty())
      entries[name] = std::move(value);
  }
  return WriteSnapshot(outputPath, "fixtures", std::move(entries));
}

// Writes a deterministic reference-only truss dictionary snapshot.
SnapshotWriteResult WriteTrussSnapshot(
    const std::filesystem::path &outputPath,
    const std::unordered_map<std::string, std::string> &dictionary) {
  std::vector<std::string> keys;
  keys.reserve(dictionary.size());
  for (const auto &[name, path] : dictionary)
    keys.push_back(name);
  std::sort(keys.begin(), keys.end());

  nlohmann::json entries = nlohmann::json::object();
  for (const auto &name : keys) {
    const std::string fileName =
        PathUtils::PathFromUtf8(dictionary.at(name)).filename().string();
    if (!fileName.empty())
      entries[name] = nlohmann::json{{"file", fileName}};
  }
  return WriteSnapshot(outputPath, "trusses", std::move(entries));
}

} // namespace DictionarySnapshotService
