#include "dictionary_reset_service.h"

#include "active_dictionary_storage.h"
#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "json.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace DictionaryResetService {
namespace {

// Returns the JSON dictionary type name for a dictionary kind.
std::string DictionaryType(ActiveDictionaryStorage::DictionaryKind kind) {
  return kind == ActiveDictionaryStorage::DictionaryKind::Fixtures ? "fixtures"
                                                                   : "trusses";
}

// Returns the file path field name for dictionary entries.
const char *FileField(ActiveDictionaryStorage::DictionaryKind kind) {
  return kind == ActiveDictionaryStorage::DictionaryKind::Fixtures ? "gdtf" : "file";
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
  const fs::path stagingJson = request.activeJsonPath.string() + ".reset.tmp";
  const fs::path stagingAssets = layout.isDefaultManagedDictionary
                                     ? layout.ownedAssetDirectory / ".reset_assets_tmp"
                                     : fs::path(layout.ownedAssetDirectory.string() + ".reset.tmp");
  RemoveIfExists(stagingJson);
  RemoveIfExists(stagingAssets);

  std::error_code ec;
  fs::create_directories(request.activeJsonPath.parent_path(), ec);
  if (!layout.isDefaultManagedDictionary)
    fs::create_directories(stagingAssets, ec);
  else
    fs::create_directories(stagingAssets, ec);
  if (ec) {
    summary.errors.push_back("Could not create dictionary reset staging storage");
    return summary;
  }

  nlohmann::json stagedEntries = nlohmann::json::object();
  for (auto it = (*entriesOpt)->begin(); it != (*entriesOpt)->end(); ++it) {
    nlohmann::json entry = it.value();
    if (!entry.is_object()) {
      ++summary.skipped_count;
      continue;
    }
    const char *fileField = FileField(request.kind);
    const auto fileIt = entry.find(fileField);
    if (fileIt == entry.end() || !fileIt->is_string()) {
      ++summary.missing_files_count;
      summary.missing_file_examples.push_back(it.key());
      continue;
    }
    const fs::path sourcePath = ActiveDictionaryStorage::ResolveReference(
        request.applicationBaseJsonPath, fileIt->get<std::string>());
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
    entry[fileField] = ActiveDictionaryStorage::MakeSerializedReference(
        stagingLayout, copied.finalPath);
    if (request.kind == ActiveDictionaryStorage::DictionaryKind::Fixtures)
      entry["sha256"] = copied.finalSha256;
    stagedEntries[it.key()] = std::move(entry);
    ++summary.added_count;
  }

  if (summary.HasErrors()) {
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
  out << DictionaryJsonContract::MakeRoot(type, std::move(stagedEntries)).dump(4);
  out.close();

  fs::path jsonBackup;
  fs::path assetBackup;
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

  fs::rename(stagingJson, request.activeJsonPath, ec);
  if (!ec) {
    if (layout.isDefaultManagedDictionary) {
      fs::create_directories(layout.ownedAssetDirectory, ec);
      for (const auto &entry : fs::directory_iterator(stagingAssets)) {
        fs::rename(entry.path(), layout.ownedAssetDirectory / entry.path().filename(), ec);
        if (ec)
          break;
      }
      RemoveIfExists(stagingAssets);
    } else {
      fs::rename(stagingAssets, layout.ownedAssetDirectory, ec);
    }
  }
  if (ec) {
    summary.errors.push_back("Could not install the reset dictionary");
    RestoreBackup(jsonBackup, request.activeJsonPath);
    RestoreBackup(assetBackup, layout.ownedAssetDirectory);
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return summary;
  }

  RemoveIfExists(jsonBackup);
  RemoveIfExists(assetBackup);
  return summary;
}

} // namespace DictionaryResetService
