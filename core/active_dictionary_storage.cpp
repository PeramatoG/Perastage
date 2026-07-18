/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "active_dictionary_storage.h"
#include "filesystem_path_utils.h"

#include <system_error>

namespace fs = std::filesystem;

namespace ActiveDictionaryStorage {
namespace {

// Returns true when both paths identify the same normalized location.
bool PathsEquivalentOrEqual(const fs::path &lhs, const fs::path &rhs) {
  if (lhs.empty() || rhs.empty())
    return false;
  const fs::path left = lhs.lexically_normal();
  const fs::path right = rhs.lexically_normal();
  if (left == right)
    return true;
  std::error_code ec;
  return fs::exists(left, ec) && !ec && fs::exists(right, ec) && !ec &&
         fs::equivalent(left, right, ec) && !ec;
}

// Returns the writable library directory for the selected dictionary kind.
fs::path GetDefaultAssetDirectory(DictionaryKind kind,
                                  const fs::path &defaultDictionaryPath) {
  (void)kind;
  return defaultDictionaryPath.parent_path();
}

} // namespace

// Returns true when the active dictionary is the managed default user file.
bool IsDefaultManagedDictionary(const fs::path &dictionaryPath,
                                const fs::path &defaultDictionaryPath) {
  return PathsEquivalentOrEqual(dictionaryPath, defaultDictionaryPath);
}

// Returns the directory where assets owned by the active dictionary are stored.
fs::path GetOwnedAssetDirectory(const fs::path &dictionaryPath,
                                const fs::path &defaultDictionaryPath) {
  if (IsDefaultManagedDictionary(dictionaryPath, defaultDictionaryPath))
    return defaultDictionaryPath.parent_path();
  return dictionaryPath.parent_path() / (dictionaryPath.stem().string() + "_assets");
}

// Builds the storage layout for an active dictionary.
AssetStorageLayout BuildLayout(DictionaryKind kind, const fs::path &dictionaryPath,
                               const fs::path &defaultDictionaryPath) {
  AssetStorageLayout layout;
  layout.dictionaryPath = dictionaryPath;
  layout.defaultDictionaryPath = defaultDictionaryPath;
  layout.isDefaultManagedDictionary =
      IsDefaultManagedDictionary(dictionaryPath, defaultDictionaryPath);
  layout.ownedAssetDirectory = layout.isDefaultManagedDictionary
                                   ? GetDefaultAssetDirectory(kind, defaultDictionaryPath)
                                   : GetOwnedAssetDirectory(dictionaryPath, defaultDictionaryPath);
  return layout;
}

// Resolves a stored dictionary reference using supported legacy and owned layouts.
fs::path ResolveReference(const fs::path &dictionaryPath,
                          const std::string &storedPath) {
  const fs::path parsedPath = PathUtils::PathFromUtf8(storedPath);
  if (parsedPath.is_absolute())
    return parsedPath;

  const fs::path jsonDir = dictionaryPath.parent_path();
  const fs::path directPath = jsonDir / parsedPath;
  if (fs::exists(directPath))
    return directPath;

  const fs::path assetPath =
      jsonDir / (dictionaryPath.stem().string() + "_assets") / parsedPath;
  if (fs::exists(assetPath))
    return assetPath;
  return directPath;
}

// Creates the path text that should be serialized for an owned asset.
std::string MakeSerializedReference(const AssetStorageLayout &layout,
                                    const fs::path &assetPath) {
  if (assetPath.empty())
    return {};
  if (!layout.isDefaultManagedDictionary &&
      PathsEquivalentOrEqual(assetPath.parent_path(), layout.ownedAssetDirectory))
    return assetPath.filename().string();
  if (layout.isDefaultManagedDictionary &&
      PathsEquivalentOrEqual(assetPath.parent_path(), layout.ownedAssetDirectory))
    return assetPath.filename().string();
  return assetPath.string();
}

// Returns the deterministic destination path for importing an owned asset.
fs::path GetAssetDestination(const AssetStorageLayout &layout,
                             const fs::path &sourcePath) {
  if (sourcePath.empty() || sourcePath.filename().empty())
    return {};
  return layout.ownedAssetDirectory / sourcePath.filename();
}

// Copies an asset into active dictionary ownership using caller-selected conflict policy.
AssetCopyResult CopyAssetIntoDictionaryStorage(const AssetCopyRequest &request) {
  AssetCopyResult result;
  if (request.sourcePath.empty() || !fs::exists(request.sourcePath))
    return result;

  const AssetStorageLayout layout = BuildLayout(
      request.kind, request.activeDictionaryPath, request.defaultDictionaryPath);
  std::error_code ec;
  fs::create_directories(layout.ownedAssetDirectory, ec);
  if (ec)
    return result;

  fs::path dest = GetAssetDestination(layout, request.sourcePath);
  if (!request.destinationFileName.empty())
    dest = layout.ownedAssetDirectory / request.destinationFileName.filename();
  const auto copyResult = FileImportUtils::CopyWithConflictPolicy(
      request.sourcePath, dest, request.conflictPolicy);
  if (!copyResult.success)
    return result;

  result.success = true;
  result.finalPath = copyResult.finalPath;
  result.serializedPath = MakeSerializedReference(layout, copyResult.finalPath);
  result.finalSha256 = copyResult.finalSha256;
  result.reusedExisting = copyResult.reusedExisting;
  return result;
}

} // namespace ActiveDictionaryStorage
