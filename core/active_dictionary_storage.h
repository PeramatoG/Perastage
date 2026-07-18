/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include "file_import_utils.h"

#include <filesystem>
#include <string>

namespace ActiveDictionaryStorage {

enum class DictionaryKind { Fixtures, Trusses };

struct AssetStorageLayout {
  std::filesystem::path dictionaryPath;
  std::filesystem::path defaultDictionaryPath;
  std::filesystem::path ownedAssetDirectory;
  bool isDefaultManagedDictionary = false;
};

struct AssetCopyRequest {
  DictionaryKind kind;
  std::filesystem::path activeDictionaryPath;
  std::filesystem::path defaultDictionaryPath;
  std::filesystem::path sourcePath;
  std::filesystem::path destinationFileName;
  FileImportUtils::ConflictPolicy conflictPolicy =
      FileImportUtils::ConflictPolicy::Rename;
};

struct AssetCopyResult {
  bool success = false;
  std::filesystem::path finalPath;
  std::string serializedPath;
  std::string finalSha256;
  bool reusedExisting = false;
};

std::filesystem::path GetOwnedAssetDirectory(
    const std::filesystem::path &dictionaryPath,
    const std::filesystem::path &defaultDictionaryPath);
bool IsDefaultManagedDictionary(
    const std::filesystem::path &dictionaryPath,
    const std::filesystem::path &defaultDictionaryPath);
AssetStorageLayout BuildLayout(DictionaryKind kind,
                               const std::filesystem::path &dictionaryPath,
                               const std::filesystem::path &defaultDictionaryPath);
std::filesystem::path ResolveReference(
    const std::filesystem::path &dictionaryPath,
    const std::string &storedPath);
std::string MakeSerializedReference(const AssetStorageLayout &layout,
                                    const std::filesystem::path &assetPath);
std::filesystem::path GetAssetDestination(const AssetStorageLayout &layout,
                                          const std::filesystem::path &sourcePath);
AssetCopyResult CopyAssetIntoDictionaryStorage(const AssetCopyRequest &request);

} // namespace ActiveDictionaryStorage
