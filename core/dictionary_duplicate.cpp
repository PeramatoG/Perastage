/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "dictionary_duplicate.h"

#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"

#include <chrono>
#include <fstream>
#include <optional>
#include <system_error>

namespace fs = std::filesystem;

namespace DictionaryDuplicate {
namespace {

// Returns a canonical path when possible and a normalized absolute path
// otherwise.
fs::path CanonicalComparablePath(const fs::path &path) {
  std::error_code ec;
  fs::path canonical = fs::weakly_canonical(path, ec);
  if (!ec)
    return canonical.lexically_normal();
  return fs::absolute(path, ec).lexically_normal();
}

// Adds an error message to the duplicate result.
void AddError(Result &result, const std::string &message) {
  result.errors.push_back(message);
}

// Returns the expected dictionary_type value for a duplicate request.
std::string ExpectedType(ActiveDictionaryStorage::DictionaryKind kind) {
  return kind == ActiveDictionaryStorage::DictionaryKind::Fixtures ? "fixtures"
                                                                   : "trusses";
}

// Reads and validates a source dictionary JSON document.
std::optional<nlohmann::json> ReadSourceJson(const Request &request,
                                             Result &result) {
  std::ifstream in(request.sourceDictionaryPath);
  if (!in.is_open()) {
    AddError(result, "Could not open the source dictionary.");
    return std::nullopt;
  }

  nlohmann::json root;
  try {
    in >> root;
  } catch (const std::exception &ex) {
    AddError(result, std::string("Could not parse the source dictionary: ") +
                         ex.what());
    return std::nullopt;
  }

  std::string error;
  if (!DictionaryJsonContract::GetEntriesForType(
          root, ExpectedType(request.kind), error)) {
    AddError(result, "The source dictionary is not valid: " + error);
    return std::nullopt;
  }
  return root;
}

// Ensures a save-dialog destination has a JSON extension.
fs::path NormalizeDestinationPath(fs::path path) {
  if (path.extension() != ".json")
    path.replace_extension(".json");
  return path;
}

// Builds a unique backup path next to an existing destination.
fs::path BuildBackupPath(const fs::path &path) {
  const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  fs::path backup = path;
  backup += ".bak." + std::to_string(stamp);
  return backup;
}

// Removes a path without reporting cleanup failures.
void RemoveIfExists(const fs::path &path) {
  if (path.empty())
    return;
  std::error_code ec;
  fs::remove_all(path, ec);
}

// Moves an existing path to a backup before replacement.
bool BackupExistingPath(const fs::path &path, fs::path &backupOut,
                        std::string &error) {
  std::error_code ec;
  if (!fs::exists(path, ec) || ec)
    return true;
  backupOut = BuildBackupPath(path);
  fs::rename(path, backupOut, ec);
  if (ec) {
    error =
        "Could not create backup for " + path.string() + ": " + ec.message();
    return false;
  }
  return true;
}

// Restores a backup path if final replacement failed.
void RestoreBackup(const fs::path &backup, const fs::path &target) {
  if (backup.empty())
    return;
  std::error_code ec;
  RemoveIfExists(target);
  fs::rename(backup, target, ec);
}

// Returns a mutable JSON object for dictionary entries.
nlohmann::json *FindEntries(nlohmann::json &root, const std::string &type) {
  if (root.value("dictionary_type", "") == "combined")
    return &root["entries"][type];
  return &root["entries"];
}

// Rewrites one serialized asset reference into the destination duplicate.
void RewriteReference(const Request &request,
                      const ActiveDictionaryStorage::AssetStorageLayout &layout,
                      const fs::path &stagingAssetsDir,
                      const std::string &entryName, nlohmann::json &entryValue,
                      Result &result) {
  std::string raw;
  if (entryValue.is_string())
    raw = entryValue.get<std::string>();
  else if (entryValue.is_object() && entryValue.contains("file") &&
           entryValue["file"].is_string())
    raw = entryValue["file"].get<std::string>();
  else if (entryValue.is_object() && entryValue.contains("path") &&
           entryValue["path"].is_string())
    raw = entryValue["path"].get<std::string>();
  if (raw.empty())
    return;

  const fs::path resolved = ActiveDictionaryStorage::ResolveReference(
      request.sourceDictionaryPath, raw);
  std::error_code ec;
  if (!fs::exists(resolved, ec) || ec) {
    ++result.unresolvedReferenceCount;
    result.unresolvedReferences.push_back(entryName + " -> " + raw);
    return;
  }

  fs::create_directories(stagingAssetsDir, ec);
  if (ec) {
    AddError(result,
             "Could not create staging asset directory: " + ec.message());
    return;
  }

  fs::path destination = stagingAssetsDir / resolved.filename();
  if (fs::exists(destination, ec) && !ec) {
    const auto sourceHash = FileImportUtils::ComputeFileSha256(resolved);
    const auto destHash = FileImportUtils::ComputeFileSha256(destination);
    if (sourceHash && destHash && *sourceHash != *destHash) {
      const std::string suffix = sourceHash->substr(0, 12);
      destination = stagingAssetsDir / (resolved.stem().string() + "_" +
                                        suffix + resolved.extension().string());
    }
  }

  fs::copy_file(resolved, destination, fs::copy_options::overwrite_existing,
                ec);
  if (ec) {
    AddError(result,
             "Could not copy asset for " + entryName + ": " + ec.message());
    return;
  }

  ++result.copiedAssetCount;
  const fs::path finalAssetPath =
      layout.ownedAssetDirectory / destination.filename();
  const std::string serialized =
      ActiveDictionaryStorage::MakeSerializedReference(layout, finalAssetPath);
  if (entryValue.is_string())
    entryValue = serialized;
  else
    entryValue[entryValue.contains("file") ? "file" : "path"] = serialized;
}

// Rewrites all object or array entries in a dictionary JSON document.
void RewriteEntries(const Request &request,
                    const ActiveDictionaryStorage::AssetStorageLayout &layout,
                    const fs::path &stagingAssetsDir, nlohmann::json &root,
                    Result &result) {
  nlohmann::json *entries = FindEntries(root, ExpectedType(request.kind));
  if (!entries)
    return;
  if (entries->is_object()) {
    for (auto it = entries->begin(); it != entries->end(); ++it)
      RewriteReference(request, layout, stagingAssetsDir, it.key(), it.value(),
                       result);
    return;
  }
  if (entries->is_array()) {
    for (size_t i = 0; i < entries->size(); ++i) {
      nlohmann::json &value = (*entries)[i];
      std::string name = "entries[" + std::to_string(i) + "]";
      if (value.is_object() && value.contains("name") &&
          value["name"].is_string())
        name = value["name"].get<std::string>();
      RewriteReference(request, layout, stagingAssetsDir, name, value, result);
    }
  }
}

// Writes JSON to disk and validates it against the dictionary contract.
bool WriteAndValidate(const Request &request, const fs::path &path,
                      const nlohmann::json &root, Result &result) {
  std::ofstream out(path);
  if (!out.is_open()) {
    AddError(result, "Could not open the staged duplicate for writing.");
    return false;
  }
  out << root.dump(4);
  out.close();

  std::string error;
  std::ifstream in(path);
  nlohmann::json check;
  try {
    in >> check;
  } catch (const std::exception &ex) {
    AddError(result,
             std::string("Could not re-read staged duplicate: ") + ex.what());
    return false;
  }
  if (!DictionaryJsonContract::GetEntriesForType(
          check, ExpectedType(request.kind), error)) {
    AddError(result, "The staged duplicate is invalid: " + error);
    return false;
  }
  return true;
}

} // namespace

// Builds a meaningful default duplicate filename next to the source dictionary.
fs::path BuildDefaultDuplicatePath(const fs::path &sourceDictionaryPath) {
  fs::path result = sourceDictionaryPath;
  result.replace_filename(sourceDictionaryPath.stem().string() + " copy.json");
  return result;
}

// Creates an independent duplicate of a fixture or truss dictionary and its
// assets.
Result DuplicateDictionary(const Request &request) {
  Result result;
  result.destinationDictionaryPath =
      NormalizeDestinationPath(request.destinationDictionaryPath);
  if (request.sourceDictionaryPath.empty() ||
      result.destinationDictionaryPath.empty()) {
    AddError(result, "Source and destination dictionary paths are required.");
    return result;
  }
  if (CanonicalComparablePath(request.sourceDictionaryPath) ==
      CanonicalComparablePath(result.destinationDictionaryPath)) {
    AddError(result,
             "Destination dictionary must be different from the source.");
    return result;
  }

  auto rootOpt = ReadSourceJson(request, result);
  if (!rootOpt)
    return result;

  const auto finalLayout = ActiveDictionaryStorage::BuildLayout(
      request.kind, result.destinationDictionaryPath,
      request.defaultDictionaryPath);
  const fs::path stagingJson =
      result.destinationDictionaryPath.string() + ".tmp";
  const fs::path stagingAssets =
      result.destinationDictionaryPath.parent_path() /
      (result.destinationDictionaryPath.stem().string() + "_assets.tmp");
  RemoveIfExists(stagingJson);
  RemoveIfExists(stagingAssets);

  std::error_code createParentError;
  fs::create_directories(result.destinationDictionaryPath.parent_path(),
                         createParentError);
  if (createParentError) {
    AddError(result, "Could not create destination directory: " +
                         createParentError.message());
    return result;
  }

  nlohmann::json root = std::move(*rootOpt);
  RewriteEntries(request, finalLayout, stagingAssets, root, result);
  if (!result.errors.empty()) {
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return result;
  }
  if (!WriteAndValidate(request, stagingJson, root, result)) {
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return result;
  }

  std::string backupError;
  if (!BackupExistingPath(result.destinationDictionaryPath,
                          result.backupDictionaryPath, backupError) ||
      !BackupExistingPath(finalLayout.ownedAssetDirectory,
                          result.backupAssetsPath, backupError)) {
    AddError(result, backupError);
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return result;
  }

  std::error_code ec;
  fs::rename(stagingJson, result.destinationDictionaryPath, ec);
  if (ec) {
    AddError(result, "Could not install duplicate dictionary: " + ec.message());
    RestoreBackup(result.backupDictionaryPath,
                  result.destinationDictionaryPath);
    RestoreBackup(result.backupAssetsPath, finalLayout.ownedAssetDirectory);
    RemoveIfExists(stagingJson);
    RemoveIfExists(stagingAssets);
    return result;
  }
  if (fs::exists(stagingAssets, ec) && !ec) {
    fs::rename(stagingAssets, finalLayout.ownedAssetDirectory, ec);
    if (ec) {
      AddError(result, "Could not install duplicate assets: " + ec.message());
      RemoveIfExists(result.destinationDictionaryPath);
      RestoreBackup(result.backupDictionaryPath,
                    result.destinationDictionaryPath);
      RestoreBackup(result.backupAssetsPath, finalLayout.ownedAssetDirectory);
      RemoveIfExists(stagingAssets);
      return result;
    }
  }

  result.success = true;
  return result;
}

} // namespace DictionaryDuplicate
