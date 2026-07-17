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
#include "trussdictionary.h"
#include "filesystem_path_utils.h"

#include "active_dictionary_storage.h"
#include "configmanager.h"
#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "json.hpp"
#include "projectutils.h"
#include "truss_gdtf_builder.h"
#include "truss.h"
#include "startup_file_access_gate.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

namespace fs = std::filesystem;

namespace TrussDictionary {

namespace {

LoadStatus g_lastLoadStatus;
constexpr const char *kTrussDictionaryPathConfigKey =
    "trusses_dictionary_active_path";

static std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

static std::string LowerExt(const fs::path &path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

static std::string TrimAsciiWhitespace(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

static std::string CollapseAsciiWhitespace(const std::string &text) {
  std::string collapsed;
  collapsed.reserve(text.size());
  bool hadWhitespace = false;
  for (unsigned char c : text) {
    if (std::isspace(c)) {
      hadWhitespace = true;
      continue;
    }
    if (hadWhitespace && !collapsed.empty())
      collapsed.push_back(' ');
    collapsed.push_back(static_cast<char>(c));
    hadWhitespace = false;
  }
  return collapsed;
}

static bool IsSupportedDictionaryFile(const fs::path &path) {
  return path.has_extension() && path.extension() == ".json";
}

static fs::path GetUserDictFile() {
  fs::path dir = PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("trusses"));
  if (dir.empty())
    return {};
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec)
    return {};
  return dir / "truss_dictionary.json";
}

static fs::path ResolveConfiguredDictionaryPath(
    const fs::path &defaultPath, const std::string &rawConfiguredPath,
    std::string *warningOut = nullptr) {
  if (warningOut)
    warningOut->clear();
  const std::string trimmedPath = TrimAsciiWhitespace(rawConfiguredPath);
  if (trimmedPath.empty())
    return defaultPath;

  fs::path configuredPath = PathUtils::PathFromUtf8(trimmedPath);
  if (configuredPath.is_relative())
    configuredPath = defaultPath.parent_path() / configuredPath;

  if (configuredPath.has_filename() && IsSupportedDictionaryFile(configuredPath)) {
    std::error_code ec;
    fs::create_directories(configuredPath.parent_path(), ec);
    if (!ec)
      return configuredPath;
    if (warningOut) {
      *warningOut =
          "Could not create dictionary directory for configured path: " +
          configuredPath.parent_path().string();
    }
    return defaultPath;
  }

  if (warningOut)
    *warningOut =
        "Configured trusses dictionary path is invalid. Expected a .json file path.";
  return defaultPath;
}

static fs::path GetConfiguredUserDictFile(std::string *warningOut = nullptr) {
  const fs::path defaultPath = GetUserDictFile();
  if (defaultPath.empty()) {
    if (warningOut)
      *warningOut = "Default trusses dictionary path is empty";
    return {};
  }

  const auto configured =
      ConfigManager::Get().GetValue(kTrussDictionaryPathConfigKey);
  if (!configured)
    return defaultPath;
  return ResolveConfiguredDictionaryPath(defaultPath, *configured, warningOut);
}

static fs::path GetBaseDictFile() {
  return ProjectUtils::GetBaseLibraryPath("trusses") / "truss_dictionary.json";
}

static bool RecreateUserDictionaryFromBase(const fs::path &userFile,
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
  create << DictionaryJsonContract::MakeRoot("trusses", nlohmann::json::object())
                .dump(4);
  return true;
}

// Copies default seed assets into a newly created custom dictionary.
static bool CopySeedAssetsIntoCustomDictionary(
    const fs::path &customFile, const std::unordered_map<std::string, std::string> &seedDict,
    std::unordered_map<std::string, std::string> &targetDict) {
  bool changed = false;
  for (const auto &[key, seedPathText] : seedDict) {
    const fs::path seedPath = PathUtils::PathFromUtf8(seedPathText);
    if (!fs::exists(seedPath))
      continue;
    const auto copied = ActiveDictionaryStorage::CopyAssetIntoDictionaryStorage(
        {ActiveDictionaryStorage::DictionaryKind::Trusses, customFile, GetUserDictFile(),
         seedPath, {}, FileImportUtils::ConflictPolicy::Rename});
    if (!copied.success)
      continue;
    targetDict[key] = copied.finalPath.string();
    changed = true;
  }
  return changed;
}

static bool WriteDictionaryBackup(const fs::path &sourceFile) {
  if (sourceFile.empty() || !fs::exists(sourceFile))
    return false;
  fs::path backupFile = sourceFile;
  backupFile += ".bak";
  std::error_code ec;
  fs::copy_file(sourceFile, backupFile, fs::copy_options::overwrite_existing, ec);
  return !ec;
}

static bool MergeSeedEntriesIntoUserDictionary(
    std::unordered_map<std::string, std::string> &userDict,
    const std::unordered_map<std::string, std::string> &baseDict,
    bool *changedOut = nullptr) {
  if (changedOut)
    *changedOut = false;

  bool changed = false;
  for (const auto &[seedKey, seedPath] : baseDict) {
    if (userDict.find(seedKey) != userDict.end())
      continue;
    userDict[seedKey] = seedPath;
    changed = true;
  }

  if (changedOut)
    *changedOut = changed;
  return true;
}

static fs::path ResolveImportedPath(const fs::path &jsonFile,
                                    const std::string &rawPathText) {
  return ActiveDictionaryStorage::ResolveReference(jsonFile, rawPathText);
}

static bool EnsureMigratedToGdtf(fs::path &pathInOut, std::string &error) {
  std::string ext = LowerExt(pathInOut);
  if (ext == ".gdtf")
    return true;

  if (ext == ".gtruss") {
    fs::path converted = pathInOut;
    converted.replace_extension(".gdtf");
    if (!fs::exists(converted) &&
        !ConvertLegacyGtrussToGdtf(pathInOut, converted, &error)) {
      return false;
    }
    pathInOut = converted;
    return true;
  }

  if (ext == ".glb" || ext == ".3ds") {
    fs::path converted = pathInOut;
    converted.replace_extension(".gdtf");
    if (!fs::exists(converted)) {
      Truss truss;
      truss.modelFile = ToUtf8String(pathInOut);
      truss.model = pathInOut.stem().string();
      if (!BuildTrussGdtfFromInstance(truss, converted, &error))
        return false;
    }
    pathInOut = converted;
    return true;
  }

  error = "Unsupported truss format";
  return false;
}

static std::optional<std::unordered_map<std::string, std::string>>
LoadFromFile(const fs::path &file, std::string &error) {
  std::unordered_map<std::string, std::string> dict;
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

  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(root, "trusses", error);
  if (!entriesOpt) {
    return std::nullopt;
  }

  const nlohmann::json &entries = **entriesOpt;
  bool changed = false;

  auto resolveEntryPath = [&file](const nlohmann::json &value, fs::path &entryPath,
                                  std::string &entryError,
                                  std::string &rawPathOut) -> bool {
    std::string pathValue;
    if (value.is_string()) {
      pathValue = value.get<std::string>();
    } else if (value.is_object()) {
      if (value.contains("file") && value["file"].is_string())
        pathValue = value["file"].get<std::string>();
      else if (value.contains("path") && value["path"].is_string())
        pathValue = value["path"].get<std::string>();
      else {
        entryError = "entry object must include string 'file' or 'path'";
        return false;
      }
    } else {
      entryError = "entry must be string or object";
      return false;
    }

    rawPathOut = pathValue;
    entryPath = ResolveImportedPath(file, pathValue);
    return true;
  };

  auto processEntry = [&](const std::string &rawKey, const nlohmann::json &value,
                          const std::string &sourceLabel) -> bool {
    const std::string normalizedKey = NormalizeModelKey(rawKey);
    if (normalizedKey.empty()) {
      error = sourceLabel + " has an empty model name";
      return false;
    }

    fs::path path;
    std::string rawPath;
    std::string entryError;
    if (!resolveEntryPath(value, path, entryError, rawPath)) {
      error = sourceLabel + " " + entryError;
      return false;
    }

    if (!fs::exists(path)) {
      if (!PathUtils::PathFromUtf8(rawPath).is_absolute()) {
        std::cerr << "Warning: trusses dictionary " << sourceLabel
                  << " references missing relative path '" << rawPath
                  << "' resolved from '" << file.string() << "'."
                  << std::endl;
      }
      return true;
    }

    std::string migrationError;
    fs::path migrated = path;
    if (!EnsureMigratedToGdtf(migrated, migrationError))
      return true;

    if (migrated != path || normalizedKey != rawKey)
      changed = true;
    dict[normalizedKey] = ToUtf8String(migrated);
    return true;
  };

  if (entries.is_object()) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
      if (!processEntry(it.key(), it.value(), "entry '" + it.key() + "'"))
        return std::nullopt;
    }
  } else {
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
      if (!processEntry(entryJson["name"].get<std::string>(), entryJson,
                        "entries[" + std::to_string(idx) + "]")) {
        return std::nullopt;
      }
    }
  }

  if (changed) {
    WriteDictionaryBackup(file);
    Save(dict);
  }
  return dict;
}

DictionaryImportSummary MergeDictionaryEntries(
    std::unordered_map<std::string, std::string> &current,
    const std::unordered_map<std::string, std::string> &imported,
    DictionaryImportPolicy policy, bool applyChanges) {
  DictionaryImportSummary summary;
  constexpr size_t kMaxMissingExamples = 5;

  for (const auto &[key, value] : imported) {
    if (value.empty())
      continue;
    std::error_code ec;
    if (fs::exists(PathUtils::PathFromUtf8(value), ec))
      continue;
    ++summary.missing_files_count;
    if (summary.missing_file_examples.size() < kMaxMissingExamples)
      summary.missing_file_examples.push_back(key + " -> " + value);
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

  for (const auto &[rawKey, value] : imported) {
    const std::string key = NormalizeModelKey(rawKey);
    if (key.empty()) {
      ++summary.skipped_count;
      continue;
    }

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

std::string NormalizeModelKey(const std::string &model) {
  std::string normalized = CollapseAsciiWhitespace(TrimAsciiWhitespace(model));
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::toupper(c));
                 });

  static const std::regex kDimensionSeparator("(\\d)\\s*[xX]\\s*(\\d)");
  normalized = std::regex_replace(normalized, kDimensionSeparator, "$1X$2");
  return normalized;
}

bool ImportTrussFile(const std::string &inputPath, std::string &storedPath,
                     std::string &error) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  storedPath.clear();
  fs::path src = PathUtils::PathFromUtf8(inputPath);
  if (!fs::exists(src)) {
    error = "Truss file does not exist";
    return false;
  }

  fs::path dictFile = GetConfiguredUserDictFile();
  if (dictFile.empty()) {
    error = "Truss dictionary is not available";
    return false;
  }

  fs::path working = src;
  if (!EnsureMigratedToGdtf(working, error))
    return false;

  const auto copyResult = ActiveDictionaryStorage::CopyAssetIntoDictionaryStorage(
      {ActiveDictionaryStorage::DictionaryKind::Trusses, dictFile, GetUserDictFile(),
       working, {}, FileImportUtils::ConflictPolicy::Rename});
  if (!copyResult.success) {
    error = "Failed to copy truss file into library";
    return false;
  }

  storedPath = ToUtf8String(copyResult.finalPath);
  return true;
}


// Validates that a trusses dictionary file can be loaded without changing configuration.
bool ValidateDictionaryFile(const std::string &path, std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  std::string loadError;
  if (!LoadFromFile(PathUtils::PathFromUtf8(path), loadError)) {
    if (errorOut)
      *errorOut = loadError;
    return false;
  }
  return true;
}

// Creates an empty trusses dictionary file with the supported JSON contract.
bool CreateEmptyDictionaryFile(const std::string &path, std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const fs::path target = PathUtils::PathFromUtf8(path);
  if (target.empty()) {
    if (errorOut)
      *errorOut = "Dictionary file path is empty";
    return false;
  }
  std::error_code ec;
  fs::create_directories(target.parent_path(), ec);
  if (ec) {
    if (errorOut)
      *errorOut = "Could not create dictionary folder: " + ec.message();
    return false;
  }
  if (fs::exists(target) && !WriteDictionaryBackup(target)) {
    if (errorOut)
      *errorOut = "Could not create a backup before replacing dictionary";
    return false;
  }
  std::ofstream out(target, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (errorOut)
      *errorOut = "Could not create dictionary file";
    return false;
  }
  out << DictionaryJsonContract::MakeRoot("trusses", nlohmann::json::object()).dump(4);
  out.close();
  return ValidateDictionaryFile(target.string(), errorOut);
}

// Creates a trusses dictionary from application defaults with copied assets.
bool CreateDictionaryFileFromDefaults(const std::string &path,
                                      std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const fs::path target = PathUtils::PathFromUtf8(path);
  if (target.empty()) {
    if (errorOut)
      *errorOut = "Dictionary file path is empty";
    return false;
  }
  if (fs::exists(target) && !WriteDictionaryBackup(target)) {
    if (errorOut)
      *errorOut = "Could not create a backup before replacing dictionary";
    return false;
  }
  std::string baseError;
  auto baseDict = LoadFromFile(GetBaseDictFile(), baseError);
  if (!baseDict) {
    if (errorOut)
      *errorOut = "Could not load default trusses dictionary: " + baseError;
    return false;
  }
  std::unordered_map<std::string, std::string> customDict = *baseDict;
  CopySeedAssetsIntoCustomDictionary(target, *baseDict, customDict);
  const auto previousValue =
      ConfigManager::Get().GetValue(kTrussDictionaryPathConfigKey);
  ConfigManager::Get().SetValue(kTrussDictionaryPathConfigKey, target.string());
  std::string saveError;
  const bool saved = Save(customDict, &saveError);
  if (previousValue)
    ConfigManager::Get().SetValue(kTrussDictionaryPathConfigKey, *previousValue);
  else
    ConfigManager::Get().RemoveKey(kTrussDictionaryPathConfigKey);
  if (!saved) {
    if (errorOut)
      *errorOut = "Could not create dictionary from defaults: " + saveError;
    return false;
  }
  return ValidateDictionaryFile(target.string(), errorOut);
}

// Returns the active trusses dictionary path.
std::string GetActiveDictionaryFilePath() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  return GetConfiguredUserDictFile().string();
}

// Returns the active trusses dictionary file name.
std::string GetActiveDictionaryFileName() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const fs::path path = GetConfiguredUserDictFile();
  if (path.empty())
    return {};
  return path.filename().string();
}

// Changes the active trusses dictionary after validating the selected file.
bool SetActiveDictionaryFilePath(const std::string &path, std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());

  const fs::path defaultPath = GetUserDictFile();
  if (defaultPath.empty()) {
    if (errorOut)
      *errorOut = "Default trusses dictionary path is empty";
    return false;
  }

  std::string warning;
  const fs::path resolvedPath =
      ResolveConfiguredDictionaryPath(defaultPath, path, &warning);
  if (!warning.empty() &&
      TrimAsciiWhitespace(path) != TrimAsciiWhitespace(defaultPath.string())) {
    if (errorOut)
      *errorOut = warning;
    return false;
  }

  if (resolvedPath == defaultPath && !fs::exists(resolvedPath))
    RecreateUserDictionaryFromBase(resolvedPath, GetBaseDictFile());

  std::string loadError;
  if (!LoadFromFile(resolvedPath, loadError)) {
    if (errorOut)
      *errorOut = "Could not load selected trusses dictionary: " + loadError;
    return false;
  }

  const auto previousValue =
      ConfigManager::Get().GetValue(kTrussDictionaryPathConfigKey);

  const std::string trimmedInput = TrimAsciiWhitespace(path);
  if (trimmedInput.empty() || resolvedPath == defaultPath) {
    ConfigManager::Get().RemoveKey(kTrussDictionaryPathConfigKey);
  } else {
    ConfigManager::Get().SetValue(kTrussDictionaryPathConfigKey, resolvedPath.string());
  }

  if (!ConfigManager::Get().SaveUserConfig()) {
    if (previousValue)
      ConfigManager::Get().SetValue(kTrussDictionaryPathConfigKey, *previousValue);
    else
      ConfigManager::Get().RemoveKey(kTrussDictionaryPathConfigKey);
    if (errorOut)
      *errorOut = "Could not persist dictionary selection in user config";
    return false;
  }
  return true;
}

std::optional<std::unordered_map<std::string, std::string>> Load() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  g_lastLoadStatus = {};

  const fs::path userFile = GetConfiguredUserDictFile(&g_lastLoadStatus.error);
  const fs::path baseFile = GetBaseDictFile();
  std::string userError;
  if (auto userDict = LoadFromFile(userFile, userError)) {
    bool mergedSeedEntries = false;
    std::string baseError;
    if (auto baseDict = LoadFromFile(baseFile, baseError)) {
      MergeSeedEntriesIntoUserDictionary(*userDict, *baseDict, &mergedSeedEntries);
      if (mergedSeedEntries) {
        WriteDictionaryBackup(userFile);
        Save(*userDict);
      }
    }
    return userDict;
  }

  std::cerr << "Warning: failed to load user truss dictionary '" << userFile.string()
            << "': " << userError << ". Falling back to base dictionary."
            << std::endl;

  std::string baseError;
  if (auto baseDict = LoadFromFile(baseFile, baseError)) {
    g_lastLoadStatus.usedDefaultDictionary = true;
    RecreateUserDictionaryFromBase(userFile, baseFile);
    return baseDict;
  }

  g_lastLoadStatus.error =
      "Failed to load user truss dictionary ('" + userFile.string() +
      "'): " + userError + ". Failed to load base truss dictionary ('" +
      baseFile.string() + "'): " + baseError;
  std::cerr << "Error: " << g_lastLoadStatus.error << std::endl;
  return std::nullopt;
}

LoadStatus GetLastLoadStatus() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  return g_lastLoadStatus;
}

bool Save(const std::unordered_map<std::string, std::string> &dict,
          std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  fs::path file = GetConfiguredUserDictFile(errorOut);
  if (file.empty()) {
    if (errorOut)
      *errorOut = "User trusses dictionary path is empty";
    return false;
  }

  nlohmann::json entries = nlohmann::json::object();
  std::unordered_map<std::string, std::string> normalizedDict;
  normalizedDict.reserve(dict.size());
  for (const auto &[model, path] : dict) {
    const std::string normalizedKey = NormalizeModelKey(model);
    if (!normalizedKey.empty())
      normalizedDict[normalizedKey] = path;
  }

  std::vector<std::string> keys;
  keys.reserve(normalizedDict.size());
  for (const auto &[model, _] : normalizedDict)
    keys.push_back(model);
  std::sort(keys.begin(), keys.end());

  const auto layout = ActiveDictionaryStorage::BuildLayout(
      ActiveDictionaryStorage::DictionaryKind::Trusses, file, GetUserDictFile());
  for (const auto &model : keys) {
    fs::path p = PathUtils::PathFromUtf8(normalizedDict.at(model));
    fs::path forced = p;
    if (forced.extension() != ".gdtf")
      forced.replace_extension(".gdtf");

    nlohmann::json entry;
    entry["file"] = ActiveDictionaryStorage::MakeSerializedReference(layout, forced);
    entry["imported_at"] = FileImportUtils::NowUtcIso8601();
    if (const auto sha = FileImportUtils::ComputeFileSha256(forced))
      entry["sha256"] = *sha;
    entries[model] = std::move(entry);
  }

  const nlohmann::json root = DictionaryJsonContract::MakeRoot("trusses", std::move(entries));
  std::ofstream out(file);
  if (!out.is_open()) {
    if (errorOut)
      *errorOut = "Could not open user trusses dictionary for writing: " +
                  file.string();
    return false;
  }
  out << root.dump(4);
  if (!out.good()) {
    if (errorOut)
      *errorOut = "Failed while writing user trusses dictionary: " +
                  file.string();
    return false;
  }
  out.flush();
  if (!out.good()) {
    if (errorOut)
      *errorOut = "Failed to flush user trusses dictionary to disk: " +
                  file.string();
    return false;
  }
  return true;
}

// Looks up a truss model in a preloaded dictionary snapshot.
std::optional<std::string> FindInLoadedDictionary(
    const std::unordered_map<std::string, std::string> &dict,
    const std::string &model, bool validateExistingPath) {
  const std::string normalizedModel = NormalizeModelKey(model);
  if (normalizedModel.empty())
    return std::nullopt;
  auto it = dict.find(normalizedModel);
  if (it == dict.end())
    return std::nullopt;
  if (validateExistingPath && !fs::exists(PathUtils::PathFromUtf8(it->second)))
    return std::nullopt;
  return it->second;
}

std::optional<std::string> Get(const std::string &model) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const std::string normalizedModel = NormalizeModelKey(model);
  if (normalizedModel.empty())
    return std::nullopt;

  auto dictOpt = Load();
  if (!dictOpt)
    return std::nullopt;

  auto &dict = *dictOpt;
  auto it = dict.find(normalizedModel);
  if (it == dict.end())
    return std::nullopt;

  if (!fs::exists(PathUtils::PathFromUtf8(it->second))) {
    dict.erase(it);
    Save(dict);
    return std::nullopt;
  }

  return it->second;
}

void Update(const std::string &model, const std::string &modelPath) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const std::string normalizedModel = NormalizeModelKey(model);
  if (normalizedModel.empty() || modelPath.empty())
    return;

  std::string storedPath;
  std::string error;
  if (!ImportTrussFile(modelPath, storedPath, error))
    return;

  auto dictOpt = Load();
  if (!dictOpt)
    return;

  auto &dict = *dictOpt;
  dict[normalizedModel] = storedPath;
  Save(dict);
}

DictionaryImportSummary PreviewImportFromFile(const std::string &filePath,
                                              DictionaryImportPolicy policy) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  DictionaryImportSummary summary;

  std::string importError;
  const fs::path importPath = PathUtils::PathFromUtf8(filePath);
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
  const fs::path importPath = PathUtils::PathFromUtf8(filePath);
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
  std::string saveError;
  if (!Save(current, &saveError))
    summary.errors.push_back("Failed to save current dictionary: " + saveError);
  return summary;
}

} // namespace TrussDictionary
