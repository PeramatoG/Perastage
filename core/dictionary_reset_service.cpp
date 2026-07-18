#include "dictionary_reset_service.h"

#include "active_dictionary_storage.h"
#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "gdtfdictionary.h"
#include "json.hpp"
#include "trussdictionary.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace DictionaryResetService {
namespace {

// Returns the JSON dictionary type name for a dictionary kind.
std::string DictionaryType(ActiveDictionaryStorage::DictionaryKind kind) {
  return kind == ActiveDictionaryStorage::DictionaryKind::Fixtures ? "fixtures"
                                                                   : "trusses";
}

// Reads a supported dictionary file reference from string or object entries.
bool ReadEntryFileReference(const nlohmann::json &entry, std::string &pathOut) {
  pathOut.clear();
  if (entry.is_string()) {
    pathOut = entry.get<std::string>();
    return !pathOut.empty();
  }
  if (!entry.is_object())
    return false;
  const auto fileIt = entry.find("file");
  if (fileIt != entry.end() && fileIt->is_string()) {
    pathOut = fileIt->get<std::string>();
    return !pathOut.empty();
  }
  const auto pathIt = entry.find("path");
  if (pathIt != entry.end() && pathIt->is_string()) {
    pathOut = pathIt->get<std::string>();
    return !pathOut.empty();
  }
  return false;
}

// Writes the canonical dictionary file reference while preserving metadata.
void WriteCanonicalFileReference(nlohmann::json &entry,
                                 const std::string &rewrittenPath) {
  if (!entry.is_object())
    entry = nlohmann::json::object();
  entry["file"] = rewrittenPath;
  if (entry.contains("path"))
    entry.erase("path");
}

// Returns a unique sibling path for transaction staging or backup files.
fs::path MakeUniqueSiblingPath(const fs::path &basePath, const std::string &suffix) {
  const auto seed = std::chrono::system_clock::now().time_since_epoch().count();
  return basePath.parent_path() /
         (basePath.filename().string() + suffix + "." + std::to_string(seed));
}

// Returns true when two existing files contain the same bytes.
bool FilesHaveSameContent(const fs::path &lhs, const fs::path &rhs) {
  std::error_code ec;
  if (!fs::exists(lhs, ec) || ec || !fs::exists(rhs, ec) || ec)
    return false;
  if (fs::file_size(lhs, ec) != fs::file_size(rhs, ec) || ec)
    return false;
  std::ifstream a(lhs, std::ios::binary);
  std::ifstream b(rhs, std::ios::binary);
  return std::equal(std::istreambuf_iterator<char>(a), {},
                    std::istreambuf_iterator<char>(b));
}

// Loads JSON from disk and reports parse failures as summary errors.
bool LoadJson(const fs::path &path, nlohmann::json &root,
              DictionaryImportSummary &summary) {
  std::ifstream in(path);
  if (!in.is_open()) {
    summary.errors.push_back("Could not open default dictionary: " + path.string());
    return false;
  }
  try {
    in >> root;
  } catch (const std::exception &e) {
    summary.errors.push_back(std::string("Could not parse default dictionary: ") + e.what());
    return false;
  }
  return true;
}

// Removes a path when it exists without surfacing cleanup failures.
void RemoveIfExists(const fs::path &path) {
  std::error_code ec;
  fs::remove_all(path, ec);
}

// Moves an existing path to a backup path.
bool BackupPath(const fs::path &path, fs::path &backupPath) {
  std::error_code ec;
  if (!fs::exists(path, ec) || ec)
    return true;
  backupPath = path;
  backupPath += ".bak";
  RemoveIfExists(backupPath);
  fs::rename(path, backupPath, ec);
  if (!ec)
    return true;
  ec.clear();
  fs::copy(path, backupPath, fs::copy_options::recursive |
                            fs::copy_options::overwrite_existing, ec);
  if (ec)
    return false;
  RemoveIfExists(path);
  return true;
}

// Restores a path from its backup.
void RestoreBackup(const fs::path &backupPath, const fs::path &targetPath) {
  if (backupPath.empty())
    return;
  RemoveIfExists(targetPath);
  std::error_code ec;
  fs::rename(backupPath, targetPath, ec);
}

} // namespace

// Replaces active dictionary contents with a self-contained copy of application defaults.
DictionaryImportSummary ResetToDefaults(const Request &request) {
  DictionaryImportSummary summary;
  if (request.activeJsonPath.empty() || request.applicationBaseJsonPath.empty()) {
    summary.errors.push_back("Dictionary reset paths are incomplete");
    return summary;
  }

  nlohmann::json baseRoot;
  if (!LoadJson(request.applicationBaseJsonPath, baseRoot, summary))
    return summary;

  std::string error;
  const std::string type = DictionaryType(request.kind);
  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(baseRoot, type, error);
  if (!entriesOpt) {
    summary.errors.push_back(error);
    return summary;
  }
  if (!(*entriesOpt)->is_object()) {
    summary.errors.push_back("Default dictionary entries must be an object");
    return summary;
  }

  const auto layout = ActiveDictionaryStorage::BuildLayout(
      request.kind, request.activeJsonPath, request.managedDefaultJsonPath);
  const fs::path stagingJson = MakeUniqueSiblingPath(request.activeJsonPath, ".reset.tmp");
  const fs::path stagingAssets =
      MakeUniqueSiblingPath(layout.ownedAssetDirectory, ".reset-assets.tmp");
  RemoveIfExists(stagingJson);
  RemoveIfExists(stagingAssets);

  std::error_code ec;
  fs::create_directories(request.activeJsonPath.parent_path(), ec);
  fs::create_directories(stagingAssets, ec);
  if (ec) {
    summary.errors.push_back("Could not create dictionary reset staging storage");
    return summary;
  }

  nlohmann::json stagedEntries = nlohmann::json::object();
  nlohmann::json validationEntries = nlohmann::json::object();
  for (auto it = (*entriesOpt)->begin(); it != (*entriesOpt)->end(); ++it) {
    nlohmann::json entry = it.value();
    if (!entry.is_object()) {
      ++summary.skipped_count;
      continue;
    }
    std::string fileReference;
    if (!ReadEntryFileReference(entry, fileReference)) {
      ++summary.missing_files_count;
      summary.missing_file_examples.push_back(it.key());
      continue;
    }
    const fs::path sourcePath = ActiveDictionaryStorage::ResolveReference(
        request.applicationBaseJsonPath, fileReference);
    if (!fs::exists(sourcePath)) {
      ++summary.missing_files_count;
      if (summary.missing_file_examples.size() < 10)
        summary.missing_file_examples.push_back(it.key() + " -> " + sourcePath.string());
      continue;
    }

    ActiveDictionaryStorage::AssetStorageLayout stagingLayout = layout;
    stagingLayout.ownedAssetDirectory = stagingAssets;
    const auto copied = FileImportUtils::CopyWithConflictPolicy(
        sourcePath, stagingAssets / sourcePath.filename(),
        FileImportUtils::ConflictPolicy::Rename);
    if (!copied.success) {
      summary.errors.push_back("Could not copy default asset: " + sourcePath.string());
      continue;
    }
    nlohmann::json validationEntry = entry;
    (void)stagingLayout;
    WriteCanonicalFileReference(validationEntry, copied.finalPath.string());
    const fs::path finalAssetPath =
        layout.ownedAssetDirectory / copied.finalPath.filename();
    WriteCanonicalFileReference(
        entry, ActiveDictionaryStorage::MakeSerializedReference(layout, finalAssetPath));
    if (request.kind == ActiveDictionaryStorage::DictionaryKind::Fixtures) {
      entry["sha256"] = copied.finalSha256;
      validationEntry["sha256"] = copied.finalSha256;
    }
    stagedEntries[it.key()] = std::move(entry);
    validationEntries[it.key()] = std::move(validationEntry);
    ++summary.added_count;
  }

  if (summary.HasErrors() || stagedEntries.empty() ||
      summary.added_count != (*entriesOpt)->size()) {
    if (!summary.HasErrors())
      summary.errors.push_back("Default dictionary reset staging lost required entries");
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return summary;
  }

  std::ofstream out(stagingJson);
  if (!out.is_open()) {
    summary.errors.push_back("Could not write staged dictionary JSON");
    RemoveIfExists(stagingAssets);
    return summary;
  }
  out << DictionaryJsonContract::MakeRoot(type, validationEntries).dump(4);
  out.close();

  summary = request.kind == ActiveDictionaryStorage::DictionaryKind::Fixtures
                ? GdtfDictionary::PreviewImportFromFile(stagingJson.string(),
                                                        DictionaryImportPolicy::ReplaceAll)
                : TrussDictionary::PreviewImportFromFile(stagingJson.string(),
                                                         DictionaryImportPolicy::ReplaceAll);
  if (summary.HasErrors() || summary.added_count == 0) {
    if (!summary.HasErrors())
      summary.errors.push_back("Validated reset dictionary is unexpectedly empty");
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return summary;
  }

  {
    std::ofstream finalOut(stagingJson, std::ios::binary | std::ios::trunc);
    if (!finalOut.is_open()) {
      summary.errors.push_back("Could not write final staged dictionary JSON");
      RemoveIfExists(stagingJson);
      RemoveIfExists(stagingAssets);
      return summary;
    }
    finalOut << DictionaryJsonContract::MakeRoot(type, stagedEntries).dump(4);
  }

  fs::path jsonBackup;
  fs::path assetBackup;
  std::vector<std::pair<fs::path, fs::path>> fileBackups;
  if (!BackupPath(request.activeJsonPath, jsonBackup) ||
      (!layout.isDefaultManagedDictionary &&
       !BackupPath(layout.ownedAssetDirectory, assetBackup))) {
    summary.errors.push_back("Could not back up the active dictionary before reset");
    RestoreBackup(jsonBackup, request.activeJsonPath);
    RestoreBackup(assetBackup, layout.ownedAssetDirectory);
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return summary;
  }

  if (layout.isDefaultManagedDictionary) {
    fs::create_directories(layout.ownedAssetDirectory, ec);
    for (const auto &entry : fs::directory_iterator(stagingAssets)) {
      const fs::path destination = layout.ownedAssetDirectory / entry.path().filename();
      if (FilesHaveSameContent(entry.path(), destination))
        continue;
      fs::path backup;
      if (!BackupPath(destination, backup)) {
        ec = std::make_error_code(std::errc::io_error);
        break;
      }
      if (!backup.empty())
        fileBackups.push_back({backup, destination});
      fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, ec);
      if (ec)
        break;
    }
    RemoveIfExists(stagingAssets);
  } else {
    fs::rename(stagingAssets, layout.ownedAssetDirectory, ec);
  }
  if (!ec) {
    RemoveIfExists(request.activeJsonPath);
    fs::rename(stagingJson, request.activeJsonPath, ec);
  }
  if (ec) {
    summary.errors.push_back("Could not install the reset dictionary");
    RestoreBackup(jsonBackup, request.activeJsonPath);
    RestoreBackup(assetBackup, layout.ownedAssetDirectory);
    for (auto it = fileBackups.rbegin(); it != fileBackups.rend(); ++it)
      RestoreBackup(it->first, it->second);
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return summary;
  }

  RemoveIfExists(jsonBackup);
  RemoveIfExists(assetBackup);
  for (const auto &backup : fileBackups)
    RemoveIfExists(backup.first);
  return summary;
}

} // namespace DictionaryResetService
