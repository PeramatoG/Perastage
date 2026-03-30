/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "gdtfdictionary.h"
#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "json.hpp"
#include "projectutils.h"
#include "startup_file_access_gate.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace GdtfDictionary {

namespace {

LoadStatus g_lastLoadStatus;

bool PathsMatchForDictionaryEntries(const std::string &lhs,
                                    const std::string &rhs) {
  if (lhs.empty() || rhs.empty())
    return false;
  const fs::path leftPath = fs::u8path(lhs).lexically_normal();
  const fs::path rightPath = fs::u8path(rhs).lexically_normal();
  if (leftPath == rightPath)
    return true;

  std::error_code ec;
  return fs::exists(leftPath, ec) && !ec && fs::exists(rightPath, ec) && !ec &&
         fs::equivalent(leftPath, rightPath, ec) && !ec;
}

bool PathsShareFileName(const std::string &lhs, const std::string &rhs) {
  if (lhs.empty() || rhs.empty())
    return false;
  const fs::path leftPath = fs::u8path(lhs);
  const fs::path rightPath = fs::u8path(rhs);
  if (leftPath.filename().empty() || rightPath.filename().empty())
    return false;
  return leftPath.filename() == rightPath.filename();
}

fs::path GetUserDictFile() {
  fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  if (dir.empty())
    return {};
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec)
    return {};
  return dir / "gdtf_dictionary.json";
}

fs::path GetBaseDictFile() {
  return ProjectUtils::GetBaseLibraryPath("fixtures") / "gdtf_dictionary.json";
}

bool RecreateUserDictionaryFromBase(const fs::path &userFile,
                                    const fs::path &baseFile) {
  if (userFile.empty() || baseFile.empty() || !fs::exists(baseFile))
    return false;
  std::error_code ec;
  fs::copy_file(baseFile, userFile, fs::copy_options::overwrite_existing, ec);
  if (!ec)
    return true;
  std::ofstream create(userFile);
  if (!create.is_open())
    return false;
  create << DictionaryJsonContract::MakeRoot("fixtures",
                                             nlohmann::json::object())
                .dump(4);
  return true;
}

fs::path ResolveImportedPath(const fs::path &jsonFile,
                             const std::string &rawPathText) {
  const fs::path parsedPath = fs::u8path(rawPathText);
  if (parsedPath.is_absolute())
    return parsedPath;

  const fs::path jsonDir = jsonFile.parent_path();
  const fs::path directPath = jsonDir / parsedPath;
  if (fs::exists(directPath))
    return directPath;

  const fs::path snapshotAssetsPath =
      jsonDir / (jsonFile.stem().string() + "_assets") / parsedPath;
  if (fs::exists(snapshotAssetsPath))
    return snapshotAssetsPath;
  return directPath;
}

std::optional<std::unordered_map<std::string, Entry>>
LoadFromFile(const fs::path &file, std::string &error) {
  std::unordered_map<std::string, Entry> dict;
  if (file.empty()) {
    error = "Dictionary file path is empty";
    return std::nullopt;
  }

  std::ifstream in(file);
  if (!in.is_open()) {
    error = "Cannot open dictionary file";
    return std::nullopt;
  }

  if (in.peek() == std::ifstream::traits_type::eof()) {
    return dict;
  }

  nlohmann::json root;
  try {
    in >> root;
  } catch (const std::exception &ex) {
    error = "Parse error: " + std::string(ex.what());
    return std::nullopt;
  }

  auto entriesOpt =
      DictionaryJsonContract::GetEntriesForType(root, "fixtures", error);
  if (!entriesOpt) {
    return std::nullopt;
  }

  const nlohmann::json &entries = **entriesOpt;
  auto parseEntryValue = [&file](const nlohmann::json &value,
                                 const std::string &entryKey, Entry &entry,
                                std::string &entryError) -> bool {
    if (value.is_string()) {
      const std::string rawPath = value.get<std::string>();
      fs::path p = ResolveImportedPath(file, rawPath);
      if (!fs::u8path(rawPath).is_absolute() && !fs::exists(p)) {
        std::cerr << "Warning: fixtures dictionary entry '" << entryKey
                  << "' references missing relative path '" << rawPath
                  << "' resolved from '" << file.string() << "'." << std::endl;
      }
      entry.path = p.string();
      return true;
    }
    if (!value.is_object()) {
      entryError = "entry must be string or object";
      return false;
    }

    std::string fname;
    if (value.contains("file") && value["file"].is_string())
      fname = value["file"].get<std::string>();
    else if (value.contains("path") && value["path"].is_string())
      fname = value["path"].get<std::string>();

    if (!fname.empty()) {
      fs::path p = ResolveImportedPath(file, fname);
      if (!fs::u8path(fname).is_absolute() && !fs::exists(p)) {
        std::cerr << "Warning: fixtures dictionary entry '" << entryKey
                  << "' references missing relative path '" << fname
                  << "' resolved from '" << file.string() << "'." << std::endl;
      }
      entry.path = p.string();
    }
    if (value.contains("mode") && value["mode"].is_string())
      entry.mode = value["mode"].get<std::string>();
    if (value.contains("category") && value["category"].is_string())
      entry.category = value["category"].get<std::string>();
    if (value.contains("source") && value["source"].is_string())
      entry.source = value["source"].get<std::string>();
    if (value.contains("imported_at") && value["imported_at"].is_string())
      entry.importedAt = value["imported_at"].get<std::string>();
    if (value.contains("sha256") && value["sha256"].is_string())
      entry.sha256 = value["sha256"].get<std::string>();
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty()) {
      entryError = "entry object must include at least one of file/path/mode/category";
      return false;
    }
    return true;
  };

  if (entries.is_object()) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
      Entry entry;
      std::string entryError;
      if (!parseEntryValue(it.value(), it.key(), entry, entryError)) {
        error = "Invalid entry '" + it.key() + "': " + entryError;
        return std::nullopt;
      }
      dict[it.key()] = entry;
    }
    return dict;
  }

  for (size_t idx = 0; idx < entries.size(); ++idx) {
    const nlohmann::json &entryJson = entries[idx];
    if (!entryJson.is_object()) {
      error = "entries[" + std::to_string(idx) + "] must be an object";
      return std::nullopt;
    }
    if (!entryJson.contains("name") || !entryJson["name"].is_string()) {
      error = "entries[" + std::to_string(idx) + "] missing string 'name'";
      return std::nullopt;
    }

    Entry entry;
    std::string entryError;
    const std::string entryName = entryJson["name"].get<std::string>();
    if (!parseEntryValue(entryJson, entryName, entry, entryError)) {
      error = "entries[" + std::to_string(idx) + "] " + entryError;
      return std::nullopt;
    }

    dict[entryName] = entry;
  }
  return dict;
}

DictionaryImportSummary MergeDictionaryEntries(
    std::unordered_map<std::string, Entry> &current,
    const std::unordered_map<std::string, Entry> &imported,
    DictionaryImportPolicy policy, bool applyChanges) {
  DictionaryImportSummary summary;
  constexpr size_t kMaxMissingExamples = 5;

  for (const auto &[key, value] : imported) {
    if (value.path.empty())
      continue;
    std::error_code ec;
    if (fs::exists(fs::u8path(value.path), ec))
      continue;
    ++summary.missing_files_count;
    if (summary.missing_file_examples.size() < kMaxMissingExamples)
      summary.missing_file_examples.push_back(key + " -> " + value.path);
  }

  if (policy == DictionaryImportPolicy::ReplaceAll) {
    if (applyChanges)
      current.clear();
    for (const auto &[key, value] : imported) {
      if (applyChanges)
        current[key] = value;
      ++summary.added_count;
    }
    return summary;
  }

  for (const auto &[key, value] : imported) {
    const auto it = current.find(key);
    if (it == current.end()) {
      if (applyChanges)
        current[key] = value;
      ++summary.added_count;
      continue;
    }

    if (policy == DictionaryImportPolicy::AddAndOverwrite) {
      if (applyChanges)
        it->second = value;
      ++summary.overwritten_count;
      continue;
    }

    ++summary.skipped_count;
  }

  return summary;
}

} // namespace

std::optional<std::unordered_map<std::string, Entry>> Load() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  g_lastLoadStatus = {};

  const fs::path userFile = GetUserDictFile();
  const fs::path baseFile = GetBaseDictFile();

  std::string userError;
  if (auto userDict = LoadFromFile(userFile, userError))
    return userDict;

  std::cerr << "Warning: failed to load user fixtures dictionary '"
            << userFile.string() << "': " << userError
            << ". Falling back to base dictionary." << std::endl;

  std::string baseError;
  if (auto baseDict = LoadFromFile(baseFile, baseError)) {
    g_lastLoadStatus.usedDefaultDictionary = true;
    RecreateUserDictionaryFromBase(userFile, baseFile);
    return baseDict;
  }

  g_lastLoadStatus.error =
      "Failed to load user fixtures dictionary ('" + userFile.string() +
      "'): " + userError + ". Failed to load base fixtures dictionary ('" +
      baseFile.string() + "'): " + baseError;
  std::cerr << "Error: " << g_lastLoadStatus.error << std::endl;
  return std::nullopt;
}

LoadStatus GetLastLoadStatus() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  return g_lastLoadStatus;
}

void Save(const std::unordered_map<std::string, Entry> &dict) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  fs::path file = GetUserDictFile();
  if (file.empty())
    return;
  nlohmann::json entries = nlohmann::json::object();
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[type, entry] : dict)
    keys.push_back(type);
  std::sort(keys.begin(), keys.end());
  for (const auto &type : keys) {
    const auto &entry = dict.at(type);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty())
      continue;
    nlohmann::json obj;
    if (!entry.path.empty()) {
      fs::path p = fs::u8path(entry.path);
      const std::string fileName = p.filename().string();
      if (!fileName.empty())
        obj["file"] = fileName;
    }
    if (!entry.mode.empty())
      obj["mode"] = entry.mode;
    if (!entry.category.empty())
      obj["category"] = entry.category;
    if (!entry.source.empty())
      obj["source"] = entry.source;
    if (!entry.importedAt.empty())
      obj["imported_at"] = entry.importedAt;
    if (!entry.sha256.empty())
      obj["sha256"] = entry.sha256;
    if (obj.empty())
      continue;
    entries[type] = obj;
  }

  const nlohmann::json root = DictionaryJsonContract::MakeRoot("fixtures", std::move(entries));
  std::ofstream out(file);
  if (!out.is_open())
    return;
  out << root.dump(4);
}

std::optional<Entry> Get(const std::string &type) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  auto dictOpt = Load();
  if (!dictOpt)
    return std::nullopt;
  auto &dict = *dictOpt;
  auto it = dict.find(type);
  if (it == dict.end())
    return std::nullopt;
  if (!it->second.path.empty() && !fs::exists(it->second.path)) {
    dict.erase(it);
    Save(dict);
    return std::nullopt;
  }
  return it->second;
}

void Update(const std::string &type, const std::string &gdtfPath, const std::string &mode, const std::string &category) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  if (type.empty() || gdtfPath.empty())
    return;
  const fs::path src = fs::u8path(gdtfPath);
  if (!fs::exists(src))
    return;
  const fs::path file = GetUserDictFile();
  if (file.empty())
    return;

  const fs::path dest = file.parent_path() / src.filename();
  const auto copyResult = FileImportUtils::CopyWithConflictPolicy(
      src, dest, FileImportUtils::ConflictPolicy::Rename);
  if (!copyResult.success)
    return;

  auto dictOpt = Load();
  if (!dictOpt)
    return; // avoid overwriting existing dictionary on load failure
  auto &dict = *dictOpt;
  Entry e;
  auto it = dict.find(type);
  if (it != dict.end())
    e = it->second;

  e.path = copyResult.finalPath.string();
  if (!mode.empty())
    e.mode = mode;
  if (!category.empty())
    e.category = category;
  e.source = src.string();
  e.importedAt = FileImportUtils::NowUtcIso8601();
  e.sha256 = copyResult.finalSha256;

  dict[type] = e;
  Save(dict);
}


void UpdateCategory(const std::string &type, const std::string &category) {
  UpdateCategoryForFile(type, {}, category);
}

void UpdateCategoryForFile(const std::string &type, const std::string &gdtfPath,
                           const std::string &category) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  if (type.empty())
    return;
  auto dictOpt = Load();
  if (!dictOpt)
    return;
  auto &dict = *dictOpt;
  auto it = dict.find(type);
  if (it == dict.end()) {
    Entry e;
    e.category = category;
    dict[type] = e;
  } else {
    std::string sharedPath = it->second.path;
    if (sharedPath.empty() && !gdtfPath.empty())
      sharedPath = gdtfPath;
    it->second.category = category;
    for (auto &[entryType, entry] : dict) {
      if (entryType == type)
        continue;
      const bool samePath = PathsMatchForDictionaryEntries(entry.path, sharedPath);
      const bool sameFileName = PathsShareFileName(entry.path, sharedPath);
      if (!samePath && !sameFileName)
        continue;
      entry.category = category;
    }
  }
  Save(dict);
}

DictionaryImportSummary PreviewImportFromFile(const std::string &filePath,
                                              DictionaryImportPolicy policy) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  DictionaryImportSummary summary;

  std::string importError;
  const fs::path importPath = fs::u8path(filePath);
  auto importedOpt = LoadFromFile(importPath, importError);
  if (!importedOpt) {
    summary.errors.push_back("Failed to load import file: " + importError);
    return summary;
  }

  auto currentOpt = Load();
  if (!currentOpt) {
    summary.errors.push_back("Failed to load current dictionary");
    return summary;
  }

  auto current = *currentOpt;
  return MergeDictionaryEntries(current, *importedOpt, policy, false);
}

DictionaryImportSummary ApplyImportFromFile(const std::string &filePath,
                                            DictionaryImportPolicy policy) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  DictionaryImportSummary summary;

  std::string importError;
  const fs::path importPath = fs::u8path(filePath);
  auto importedOpt = LoadFromFile(importPath, importError);
  if (!importedOpt) {
    summary.errors.push_back("Failed to load import file: " + importError);
    return summary;
  }

  auto currentOpt = Load();
  if (!currentOpt) {
    summary.errors.push_back("Failed to load current dictionary");
    return summary;
  }

  auto current = *currentOpt;
  summary = MergeDictionaryEntries(current, *importedOpt, policy, true);
  Save(current);
  return summary;
}

} // namespace GdtfDictionary
