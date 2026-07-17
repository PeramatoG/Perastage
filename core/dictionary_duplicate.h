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

#include "active_dictionary_storage.h"

#include <filesystem>
#include <string>
#include <vector>

namespace DictionaryDuplicate {

struct Request {
  ActiveDictionaryStorage::DictionaryKind kind;
  std::filesystem::path sourceDictionaryPath;
  std::filesystem::path defaultDictionaryPath;
  std::filesystem::path destinationDictionaryPath;
};

struct Result {
  bool success = false;
  std::filesystem::path destinationDictionaryPath;
  std::filesystem::path backupDictionaryPath;
  std::filesystem::path backupAssetsPath;
  size_t copiedAssetCount = 0;
  size_t unresolvedReferenceCount = 0;
  std::vector<std::string> unresolvedReferences;
  std::vector<std::string> errors;
};

std::filesystem::path
BuildDefaultDuplicatePath(const std::filesystem::path &sourceDictionaryPath);
Result DuplicateDictionary(const Request &request);

} // namespace DictionaryDuplicate
