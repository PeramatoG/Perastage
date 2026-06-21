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
#include "configmanager.h"
#include "dictionary_json_contract.h"
#include "file_import_utils.h"
#include "filesystem_path_utils.h"
#include "json.hpp"
#include "projectutils.h"
#include "startup_file_access_gate.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tinyxml2.h>
#include <vector>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace GdtfDictionary {

namespace {

LoadStatus g_lastLoadStatus;
size_t g_saveCallCountForTesting = 0;
fs::path GetUserDictFile();

constexpr const char *kFixturesDictionaryPathConfigKey =
    "fixtures_dictionary_active_path";

std::string TrimAsciiWhitespace(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

bool IsSupportedDictionaryFile(const fs::path &path) {
  return path.has_extension() && path.extension() == ".json";
}

fs::path ResolveConfiguredDictionaryPath(const fs::path &defaultPath,
                                         const std::string &rawConfiguredPath,
                                         std::string *warningOut = nullptr) {
  if (warningOut)
    warningOut->clear();
  const std::string trimmedPath = TrimAsciiWhitespace(rawConfiguredPath);
  if (trimmedPath.empty())
    return defaultPath;

  fs::path configuredPath = PathUtils::PathFromUtf8(trimmedPath);
  if (configuredPath.is_relative()) {
    configuredPath = defaultPath.parent_path() / configuredPath;
  }

  if (configuredPath.has_filename() &&
      IsSupportedDictionaryFile(configuredPath)) {
    std::error_code ec;
    fs::create_directories(configuredPath.parent_path(), ec);
    if (!ec)
      return configuredPath;
    if (warningOut)
      *warningOut =
          "Could not create dictionary directory for configured path: " +
                    configuredPath.parent_path().string();
    return defaultPath;
  }

  if (warningOut)
    *warningOut = "Configured fixtures dictionary path is invalid. Expected a "
                  ".json file path.";
  return defaultPath;
}

fs::path GetConfiguredUserDictFile(std::string *warningOut = nullptr) {
  const fs::path defaultPath = GetUserDictFile();
  if (defaultPath.empty()) {
    if (warningOut)
      *warningOut = "Default fixtures dictionary path is empty";
    return {};
  }

  const auto configured =
      ConfigManager::Get().GetValue(kFixturesDictionaryPathConfigKey);
  if (!configured)
    return defaultPath;
  return ResolveConfiguredDictionaryPath(defaultPath, *configured, warningOut);
}

bool PathsMatchForDictionaryEntries(const std::string &lhs,
                                    const std::string &rhs) {
  if (lhs.empty() || rhs.empty())
    return false;
  const fs::path leftPath = PathUtils::PathFromUtf8(lhs).lexically_normal();
  const fs::path rightPath = PathUtils::PathFromUtf8(rhs).lexically_normal();
  if (leftPath == rightPath)
    return true;

  std::error_code ec;
  return fs::exists(leftPath, ec) && !ec && fs::exists(rightPath, ec) && !ec &&
         fs::equivalent(leftPath, rightPath, ec) && !ec;
}

bool PathsShareFileName(const std::string &lhs, const std::string &rhs) {
  if (lhs.empty() || rhs.empty())
    return false;
  const fs::path leftPath = PathUtils::PathFromUtf8(lhs);
  const fs::path rightPath = PathUtils::PathFromUtf8(rhs);
  if (leftPath.filename().empty() || rightPath.filename().empty())
    return false;
  return leftPath.filename() == rightPath.filename();
}

std::string NormalizeAsciiKey(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char ch) {
                               return std::isspace(ch) != 0 || ch == '_' ||
                                      ch == '-';
                             }),
              value.end());
  return value;
}

std::optional<std::string> NormalizeHexColor(std::string raw) {
  raw.erase(
      std::remove_if(raw.begin(), raw.end(),
                     [](unsigned char ch) { return std::isspace(ch) != 0; }),
            raw.end());
  if (raw.empty())
    return std::string();
  if (raw.front() == '#')
    raw.erase(raw.begin());
  if (raw.size() != 6)
    return std::nullopt;
  for (char &ch : raw) {
    if (!std::isxdigit(static_cast<unsigned char>(ch)))
      return std::nullopt;
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return "#" + raw;
}

std::optional<std::string>
GetObjectStringByNormalizedKey(const nlohmann::json &value,
                               const std::string &key) {
  if (!value.is_object())
    return std::nullopt;
  const std::string normalizedTarget = NormalizeAsciiKey(key);
  for (auto it = value.begin(); it != value.end(); ++it) {
    if (!it.value().is_string())
      continue;
    if (NormalizeAsciiKey(it.key()) != normalizedTarget)
      continue;
    return it.value().get<std::string>();
  }
  return std::nullopt;
}

std::string NormalizeModeKey(const std::string &mode) {
  return NormalizeAsciiKey(mode);
}

bool ModesMatchForDictionaryEntries(const std::string &lhs,
                                    const std::string &rhs) {
  return NormalizeModeKey(lhs) == NormalizeModeKey(rhs);
}

bool EntriesShareFixtureFamily(const Entry &entry,
                               const std::string &referencePath,
                               const std::string &referenceMode) {
  const bool samePath =
      PathsMatchForDictionaryEntries(entry.path, referencePath);
  const bool sameFileName = PathsShareFileName(entry.path, referencePath);
  if (!samePath && !sameFileName)
    return false;
  return ModesMatchForDictionaryEntries(entry.mode, referenceMode);
}

void HarmonizeColorsByFixtureFamily(
    std::unordered_map<std::string, Entry> &dict) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[key, _] : dict)
    keys.push_back(key);
  std::sort(keys.begin(), keys.end());

  for (const auto &sourceKey : keys) {
    const auto sourceIt = dict.find(sourceKey);
    if (sourceIt == dict.end())
      continue;
    const Entry &source = sourceIt->second;
    if (source.path.empty() || source.mode.empty() ||
        source.visualColorHex.empty())
      continue;
    for (auto &[targetKey, target] : dict) {
      if (targetKey == sourceKey)
        continue;
      if (!EntriesShareFixtureFamily(target, source.path, source.mode))
        continue;
      target.visualColorHex = source.visualColorHex;
    }
  }
}

bool IsDummy1ChFallbackType(const std::string &type) {
  return NormalizeAsciiKey(type) == "dummy1ch";
}

bool IsDummy1ChFallbackPath(const std::string &gdtfPath) {
  if (gdtfPath.empty())
    return false;
  const std::string fileName =
      PathUtils::PathFromUtf8(gdtfPath).filename().string();
  const std::string normalizedFileName = NormalizeAsciiKey(fileName);
  return normalizedFileName == "dummy1ch.gdtf" ||
         normalizedFileName == "generic1ch.gdtf" ||
         normalizedFileName == "perastagedummy1chperastage.gdtf" ||
         normalizedFileName == "unknowndummy1chperastage.gdtf" ||
         normalizedFileName == "genericgeneric1chperastage.gdtf";
}

std::string NormalizeTypeKey(const std::string &type) {
  std::string normalized;
  normalized.reserve(type.size());
  for (unsigned char ch : type) {
    if (std::isspace(ch) != 0)
      continue;
    normalized.push_back(static_cast<char>(std::toupper(ch)));
  }
  return normalized;
}

// Returns true when the filename optional-comment segment marks a
// Perastage-authored GDTF.
bool IsPerastageNamedGdtfFilePath(const std::filesystem::path &path) {
  const std::string stem = path.stem().string();
  const size_t firstAt = stem.find('@');
  if (firstAt == std::string::npos)
    return false;
  const size_t secondAt = stem.find('@', firstAt + 1);
  if (secondAt == std::string::npos)
    return false;
  const std::string comment = stem.substr(secondAt + 1);
  std::string normalized = NormalizeAsciiKey(comment);
  return normalized == "perastage";
}

// Loads manufacturer and fixture type from a GDTF description.xml payload when
// available.
bool TryReadGdtfIdentityFromDescription(const std::filesystem::path &sourcePath,
                                        std::string &manufacturerOut,
                                        std::string &fixtureTypeOut) {
  manufacturerOut.clear();
  fixtureTypeOut.clear();
  wxFileInputStream input(wxString::FromUTF8(sourcePath.string()));
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string descriptionXml;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    const fs::path entryPath(entry->GetName().ToStdString());
    if (NormalizeAsciiKey(entryPath.filename().string()) != "description.xml")
      continue;
    char buffer[4096];
    while (true) {
      zipInput.Read(buffer, sizeof(buffer));
      const size_t count = zipInput.LastRead();
      if (count == 0)
        break;
      descriptionXml.append(buffer, buffer + count);
    }
    break;
  }
  if (descriptionXml.empty())
    return false;

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.c_str(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS)
    return false;

  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  tinyxml2::XMLElement *fixtureType =
      root ? root->FirstChildElement("FixtureType")
           : doc.FirstChildElement("FixtureType");
  if (!fixtureType)
    return false;

  const char *manufacturer = fixtureType->Attribute("Manufacturer");
  const char *fixtureName = fixtureType->Attribute("Name");
  manufacturerOut = TrimAsciiWhitespace(manufacturer ? manufacturer : "");
  fixtureTypeOut = TrimAsciiWhitespace(fixtureName ? fixtureName : "");
  return !manufacturerOut.empty() || !fixtureTypeOut.empty();
}

// Builds a canonical Perastage export filename using parsed GDTF identity
// values.
std::string
BuildPerastageCanonicalGdtfFileName(const std::filesystem::path &sourcePath) {
  std::string manufacturerName;
  std::string fixtureTypeName;
  TryReadGdtfIdentityFromDescription(sourcePath, manufacturerName,
                                     fixtureTypeName);

  if (manufacturerName.empty())
    manufacturerName = "Unknown";
  if (fixtureTypeName.empty())
    fixtureTypeName = sourcePath.stem().string();

  std::replace(manufacturerName.begin(), manufacturerName.end(), '@', '_');
  std::replace(manufacturerName.begin(), manufacturerName.end(), ' ', '_');
  std::replace(fixtureTypeName.begin(), fixtureTypeName.end(), '@', '_');
  std::replace(fixtureTypeName.begin(), fixtureTypeName.end(), ' ', '_');
  manufacturerName = TrimAsciiWhitespace(manufacturerName);
  fixtureTypeName = TrimAsciiWhitespace(fixtureTypeName);
  if (manufacturerName.empty())
    manufacturerName = "Unknown";
  if (fixtureTypeName.empty())
    fixtureTypeName = sourcePath.stem().string();
  return manufacturerName + "@" + fixtureTypeName + "@Perastage.gdtf";
}

std::optional<std::string>
FindEquivalentTypeKey(const std::unordered_map<std::string, Entry> &dict,
                      const std::string &rawType) {
  const std::string normalizedTarget = NormalizeTypeKey(rawType);
  if (normalizedTarget.empty())
    return std::nullopt;

  for (const auto &[existingType, _] : dict) {
    if (NormalizeTypeKey(existingType) == normalizedTarget)
      return existingType;
  }
  return std::nullopt;
}

bool ApplyCategoryUpdateForFile(std::unordered_map<std::string, Entry> &dict,
                                const std::string &type,
                                const std::string &gdtfPath,
                                const std::string &category) {
  const std::string normalizedType = NormalizeTypeKey(type);
  if (normalizedType.empty())
    return false;

  std::string keyToUse = type;
  if (auto existingKey = FindEquivalentTypeKey(dict, normalizedType))
    keyToUse = *existingKey;
  auto it = dict.find(keyToUse);
  if (it == dict.end()) {
    Entry e;
    e.category = category;
    dict[keyToUse] = e;
    return true;
  }

  std::string sharedPath = it->second.path;
  if (sharedPath.empty() && !gdtfPath.empty())
    sharedPath = gdtfPath;
  it->second.category = category;
  for (auto &[entryType, entry] : dict) {
    if (entryType == keyToUse)
      continue;
    const bool samePath =
        PathsMatchForDictionaryEntries(entry.path, sharedPath);
    const bool sameFileName = PathsShareFileName(entry.path, sharedPath);
    if (!samePath && !sameFileName)
      continue;
    entry.category = category;
  }
  return true;
}

fs::path GetUserDictFile() {
  fs::path dir =
      PathUtils::PathFromUtf8(ProjectUtils::GetWritableLibraryPath("fixtures"));
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

bool WriteDictionaryBackup(const fs::path &sourceFile) {
  if (sourceFile.empty() || !fs::exists(sourceFile))
    return false;
  fs::path backupFile = sourceFile;
  backupFile += ".bak";
  std::error_code ec;
  fs::copy_file(sourceFile, backupFile, fs::copy_options::overwrite_existing,
                ec);
  return !ec;
}

bool MergeSeedEntriesIntoUserDictionary(
    std::unordered_map<std::string, Entry> &userDict,
    const std::unordered_map<std::string, Entry> &baseDict,
    bool *changedOut = nullptr) {
  if (changedOut)
    *changedOut = false;

  bool changed = false;
  for (const auto &[seedKey, seedEntry] : baseDict) {
    if (userDict.find(seedKey) != userDict.end())
      continue;
    userDict[seedKey] = seedEntry;
    changed = true;
  }

  if (changedOut)
    *changedOut = changed;
  return true;
}

fs::path ResolveImportedPath(const fs::path &jsonFile,
                             const std::string &rawPathText) {
  const fs::path parsedPath = PathUtils::PathFromUtf8(rawPathText);
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
      if (!PathUtils::PathFromUtf8(rawPath).is_absolute() && !fs::exists(p)) {
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
    if (const auto fileName = GetObjectStringByNormalizedKey(value, "file"))
      fname = *fileName;
    else if (const auto pathName =
                 GetObjectStringByNormalizedKey(value, "path"))
      fname = *pathName;

    if (!fname.empty()) {
      fs::path p = ResolveImportedPath(file, fname);
      if (!PathUtils::PathFromUtf8(fname).is_absolute() && !fs::exists(p)) {
        std::cerr << "Warning: fixtures dictionary entry '" << entryKey
                  << "' references missing relative path '" << fname
                  << "' resolved from '" << file.string() << "'." << std::endl;
      }
      entry.path = p.string();
    }
    if (const auto modeText = GetObjectStringByNormalizedKey(value, "mode"))
      entry.mode = *modeText;
    if (const auto categoryText =
            GetObjectStringByNormalizedKey(value, "category"))
      entry.category = *categoryText;
    auto colorText = GetObjectStringByNormalizedKey(value, "visual_color");
    if (!colorText)
      colorText = GetObjectStringByNormalizedKey(value, "color");
    if (colorText) {
      const auto normalizedColor = NormalizeHexColor(*colorText);
      if (!normalizedColor.has_value()) {
        entryError = "visual_color must be #RRGGBB when provided";
        return false;
      }
      entry.visualColorHex = *normalizedColor;
    }
    if (const auto importedAtText =
            GetObjectStringByNormalizedKey(value, "imported_at"))
      entry.importedAt = *importedAtText;
    if (const auto shaText = GetObjectStringByNormalizedKey(value, "sha256"))
      entry.sha256 = *shaText;
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty() &&
        entry.visualColorHex.empty()) {
      entryError = "entry object must include at least one of "
                   "file/path/mode/category/visual_color";
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
    HarmonizeColorsByFixtureFamily(dict);
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
  HarmonizeColorsByFixtureFamily(dict);
  return dict;
}

DictionaryImportSummary
MergeDictionaryEntries(std::unordered_map<std::string, Entry> &current,
    const std::unordered_map<std::string, Entry> &imported,
    DictionaryImportPolicy policy, bool applyChanges) {
  DictionaryImportSummary summary;
  constexpr size_t kMaxMissingExamples = 5;

  for (const auto &[key, value] : imported) {
    if (value.path.empty())
      continue;
    std::error_code ec;
    if (fs::exists(PathUtils::PathFromUtf8(value.path), ec))
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
    const auto resolvedKey = FindEquivalentTypeKey(current, key);
    const auto it = resolvedKey ? current.find(*resolvedKey) : current.end();
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

std::string GetActiveDictionaryFilePath() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  return GetConfiguredUserDictFile().string();
}

std::string GetActiveDictionaryFileName() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const fs::path path = GetConfiguredUserDictFile();
  if (path.empty())
    return {};
  return path.filename().string();
}

bool SetActiveDictionaryFilePath(const std::string &path,
                                 std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());

  const fs::path defaultPath = GetUserDictFile();
  if (defaultPath.empty()) {
    if (errorOut)
      *errorOut = "Default fixtures dictionary path is empty";
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

  const auto previousValue =
      ConfigManager::Get().GetValue(kFixturesDictionaryPathConfigKey);

  const std::string trimmedInput = TrimAsciiWhitespace(path);
  if (trimmedInput.empty() || resolvedPath == defaultPath) {
    ConfigManager::Get().RemoveKey(kFixturesDictionaryPathConfigKey);
  } else {
    ConfigManager::Get().SetValue(kFixturesDictionaryPathConfigKey,
                                  resolvedPath.string());
  }

  if (!ConfigManager::Get().SaveUserConfig()) {
    if (previousValue)
      ConfigManager::Get().SetValue(kFixturesDictionaryPathConfigKey,
                                    *previousValue);
    else
      ConfigManager::Get().RemoveKey(kFixturesDictionaryPathConfigKey);
    if (errorOut)
      *errorOut = "Could not persist dictionary selection in user config";
    return false;
  }

  if (!fs::exists(resolvedPath)) {
    if (!RecreateUserDictionaryFromBase(resolvedPath, GetBaseDictFile())) {
      std::string createError;
      if (!Save({}, &createError)) {
        if (previousValue)
          ConfigManager::Get().SetValue(kFixturesDictionaryPathConfigKey,
                                        *previousValue);
        else
          ConfigManager::Get().RemoveKey(kFixturesDictionaryPathConfigKey);
        ConfigManager::Get().SaveUserConfig();
        if (errorOut)
          *errorOut = "Could not create selected fixtures dictionary file: " +
                      createError;
        return false;
      }
    }
  }

  std::string loadError;
  if (!LoadFromFile(resolvedPath, loadError)) {
    if (previousValue)
      ConfigManager::Get().SetValue(kFixturesDictionaryPathConfigKey,
                                    *previousValue);
    else
      ConfigManager::Get().RemoveKey(kFixturesDictionaryPathConfigKey);
    ConfigManager::Get().SaveUserConfig();
    if (errorOut)
      *errorOut = "Could not load selected fixtures dictionary: " + loadError;
    return false;
  }

  return true;
}

std::optional<std::unordered_map<std::string, Entry>> Load() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  g_lastLoadStatus = {};

  const fs::path userFile = GetConfiguredUserDictFile(&g_lastLoadStatus.error);
  const fs::path baseFile = GetBaseDictFile();

  std::string userError;
  if (auto userDict = LoadFromFile(userFile, userError)) {
    bool mergedSeedEntries = false;
    std::string baseError;
    if (auto baseDict = LoadFromFile(baseFile, baseError)) {
      MergeSeedEntriesIntoUserDictionary(*userDict, *baseDict,
                                         &mergedSeedEntries);
      if (mergedSeedEntries) {
        WriteDictionaryBackup(userFile);
        Save(*userDict);
      }
    }
    return userDict;
  }

  std::cerr << "Warning: failed to load user fixtures dictionary '"
            << userFile.string() << "': " << userError
            << ". Falling back to base dictionary." << std::endl;

  std::string baseError;
  if (auto baseDict = LoadFromFile(baseFile, baseError)) {
    g_lastLoadStatus.usedDefaultDictionary = true;
    RecreateUserDictionaryFromBase(userFile, baseFile);
    return baseDict;
  }

  g_lastLoadStatus.error = "Failed to load user fixtures dictionary ('" +
                           userFile.string() + "'): " + userError +
                           ". Failed to load base fixtures dictionary ('" +
      baseFile.string() + "'): " + baseError;
  std::cerr << "Error: " << g_lastLoadStatus.error << std::endl;
  return std::nullopt;
}

LoadStatus GetLastLoadStatus() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  return g_lastLoadStatus;
}

bool Save(const std::unordered_map<std::string, Entry> &dict,
          std::string *errorOut) {
  if (errorOut)
    errorOut->clear();
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  fs::path file = GetConfiguredUserDictFile(errorOut);
  if (file.empty()) {
    if (errorOut)
      *errorOut = "User fixtures dictionary path is empty";
    return false;
  }
  nlohmann::json entries = nlohmann::json::object();
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[type, entry] : dict)
    keys.push_back(type);
  std::sort(keys.begin(), keys.end());
  for (const auto &type : keys) {
    const auto &entry = dict.at(type);
    if (entry.path.empty() && entry.mode.empty() && entry.category.empty() &&
        entry.visualColorHex.empty())
      continue;
    nlohmann::json obj;
    if (!entry.path.empty()) {
      fs::path p = PathUtils::PathFromUtf8(entry.path);
      const std::string fileName = p.filename().string();
      if (!fileName.empty())
        obj["file"] = fileName;
    }
    if (!entry.mode.empty())
      obj["mode"] = entry.mode;
    if (!entry.category.empty())
      obj["category"] = entry.category;
    if (!entry.visualColorHex.empty())
      obj["visual_color"] = entry.visualColorHex;
    if (!entry.importedAt.empty())
      obj["imported_at"] = entry.importedAt;
    if (!entry.sha256.empty())
      obj["sha256"] = entry.sha256;
    if (obj.empty())
      continue;
    entries[type] = obj;
  }

  const nlohmann::json root =
      DictionaryJsonContract::MakeRoot("fixtures", std::move(entries));
  std::ofstream out(file);
  if (!out.is_open()) {
    if (errorOut)
      *errorOut = "Could not open user fixtures dictionary for writing: " +
                  file.string();
    return false;
  }
  out << root.dump(4);
  if (!out.good()) {
    if (errorOut)
      *errorOut =
          "Failed while writing user fixtures dictionary: " + file.string();
    return false;
  }
  out.flush();
  if (!out.good()) {
    if (errorOut)
      *errorOut =
          "Failed to flush user fixtures dictionary to disk: " + file.string();
    return false;
  }
  ++g_saveCallCountForTesting;
  return true;
}

// Looks up a fixture type in a preloaded dictionary snapshot.
std::optional<Entry>
FindInLoadedDictionary(const std::unordered_map<std::string, Entry> &dict,
    const std::string &type, bool validateExistingPath) {
  const std::string normalizedType = NormalizeTypeKey(type);
  if (normalizedType.empty())
    return std::nullopt;
  auto keyOpt = FindEquivalentTypeKey(dict, normalizedType);
  if (!keyOpt)
    return std::nullopt;
  auto it = dict.find(*keyOpt);
  if (it == dict.end())
    return std::nullopt;
  if (validateExistingPath && !it->second.path.empty() &&
      !fs::exists(it->second.path))
    return std::nullopt;
  return it->second;
}

std::optional<Entry> Get(const std::string &type) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const std::string normalizedType = NormalizeTypeKey(type);
  if (normalizedType.empty())
    return std::nullopt;
  auto dictOpt = Load();
  if (!dictOpt)
    return std::nullopt;
  auto &dict = *dictOpt;
  auto keyOpt = FindEquivalentTypeKey(dict, normalizedType);
  if (!keyOpt)
    return std::nullopt;
  auto it = dict.find(*keyOpt);
  if (!it->second.path.empty() && !fs::exists(it->second.path)) {
    dict.erase(it);
    Save(dict);
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::string>
GetDefaultVisualColorForFixture(const std::string &type,
                                const std::string &gdtfPath,
    const std::string &mode) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  auto dictOpt = Load();
  if (!dictOpt)
    return std::nullopt;
  const auto &dict = *dictOpt;

  const std::string normalizedType = NormalizeTypeKey(type);
  if (!normalizedType.empty()) {
    if (auto keyOpt = FindEquivalentTypeKey(dict, normalizedType)) {
      auto byType = dict.find(*keyOpt);
      if (byType != dict.end() && !byType->second.visualColorHex.empty())
        return byType->second.visualColorHex;
    }
  }

  std::optional<std::string> matchedColor;
  std::string matchedKey;
  for (const auto &[entryType, entry] : dict) {
    if (entry.visualColorHex.empty())
      continue;
    if (!EntriesShareFixtureFamily(entry, gdtfPath, mode))
      continue;
    if (!matchedColor || entryType < matchedKey) {
      matchedColor = entry.visualColorHex;
      matchedKey = entryType;
    }
  }
  return matchedColor;
}

// Updates one dictionary entry without performing any fixture file copies.
void UpdateDictionaryEntry(const std::string &type, const Entry &entry) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const std::string normalizedType = NormalizeTypeKey(type);
  if (normalizedType.empty())
    return;

  auto dictOpt = Load();
  if (!dictOpt)
    return;
  auto &dict = *dictOpt;
  std::string keyToUse = type;
  if (auto existingKey = FindEquivalentTypeKey(dict, normalizedType))
    keyToUse = *existingKey;
  dict[keyToUse] = entry;
  Save(dict);
}

// Builds the canonical @Perastage derivative filename for a GDTF file.
std::string BuildPerastageCanonicalGdtfFileName(const std::string &gdtfPath) {
  return BuildPerastageCanonicalGdtfFileName(PathUtils::PathFromUtf8(gdtfPath));
}

// Returns true when a GDTF path already has canonical Perastage derivative naming.
bool IsPerastageNamedGdtfFile(const std::string &gdtfPath) {
  if (gdtfPath.empty())
    return false;
  return IsPerastageNamedGdtfFilePath(PathUtils::PathFromUtf8(gdtfPath));
}

// Creates or refreshes a stable @Perastage derivative for a library-owned GDTF.
std::optional<Entry> CreateOrUpdatePerastageLibraryDerivative(
    const std::string &type, const std::string &gdtfPath,
    const std::string &mode, const std::string &category) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const std::string normalizedType = NormalizeTypeKey(type);
  if (normalizedType.empty() || gdtfPath.empty())
    return std::nullopt;
  if (IsDummy1ChFallbackType(normalizedType) ||
      IsDummy1ChFallbackPath(gdtfPath))
    return std::nullopt;

  const fs::path src = PathUtils::PathFromUtf8(gdtfPath);
  if (!fs::exists(src))
    return std::nullopt;
  const fs::path file = GetConfiguredUserDictFile();
  if (file.empty())
    return std::nullopt;

  const fs::path dest =
      IsPerastageNamedGdtfFilePath(src)
          ? file.parent_path() / src.filename()
          : file.parent_path() /
                BuildPerastageCanonicalGdtfFileName(src.string());
  const auto copyResult = FileImportUtils::CopyWithConflictPolicy(
      src, dest, FileImportUtils::ConflictPolicy::Overwrite);
  if (!copyResult.success)
    return std::nullopt;

  Entry e;
  if (auto existing = Get(type))
    e = *existing;
  e.path = copyResult.finalPath.string();
  if (!mode.empty())
    e.mode = mode;
  if (!category.empty())
    e.category = category;
  e.importedAt = FileImportUtils::NowUtcIso8601();
  e.sha256 = copyResult.finalSha256;
  UpdateDictionaryEntry(type, e);
  return e;
}

// Registers an explicit user-library fixture import using deterministic
// @Perastage derivative rules.
void Update(const std::string &type, const std::string &gdtfPath,
            const std::string &mode, const std::string &category) {
  (void)CreateOrUpdatePerastageLibraryDerivative(type, gdtfPath, mode,
                                                 category);
}

void UpdateCategory(const std::string &type, const std::string &category) {
  UpdateCategoryForFile(type, {}, category);
}

void UpdateCategoryForFile(const std::string &type, const std::string &gdtfPath,
                           const std::string &category) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  auto dictOpt = Load();
  if (!dictOpt)
    return;
  auto &dict = *dictOpt;
  if (!ApplyCategoryUpdateForFile(dict, type, gdtfPath, category))
    return;
  Save(dict);
}

void UpdateCategoriesBulk(
    const std::unordered_map<std::string, std::string> &categoriesByType) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  if (categoriesByType.empty())
    return;
  auto dictOpt = Load();
  if (!dictOpt)
    return;
  auto &dict = *dictOpt;
  bool hasValidUpdate = false;
  for (const auto &[type, category] : categoriesByType) {
    if (!ApplyCategoryUpdateForFile(dict, type, {}, category))
      continue;
    hasValidUpdate = true;
  }
  if (!hasValidUpdate)
    return;
  Save(dict);
}

void UpdateVisualColor(const std::string &type, const std::string &color) {
  UpdateVisualColorForFile(type, {}, {}, color);
}

void UpdateVisualColorForFile(const std::string &type,
                              const std::string &gdtfPath,
                              const std::string &mode,
                              const std::string &color) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  const std::string normalizedType = NormalizeTypeKey(type);
  if (normalizedType.empty())
    return;
  auto dictOpt = Load();
  if (!dictOpt)
    return;
  auto &dict = *dictOpt;
  std::string keyToUse = type;
  if (auto existingKey = FindEquivalentTypeKey(dict, normalizedType))
    keyToUse = *existingKey;
  auto it = dict.find(keyToUse);
  std::string sharedPath = gdtfPath;
  std::string sharedMode = mode;
  if (it == dict.end()) {
    Entry e;
    if (!gdtfPath.empty())
      e.path = gdtfPath;
    if (!mode.empty())
      e.mode = mode;
    e.visualColorHex = color;
    dict[keyToUse] = e;
  } else {
    if (sharedPath.empty())
      sharedPath = it->second.path;
    if (sharedMode.empty())
      sharedMode = it->second.mode;
    if (!gdtfPath.empty())
      it->second.path = gdtfPath;
    if (!mode.empty())
      it->second.mode = mode;
    it->second.visualColorHex = color;
  }

  if (sharedMode.empty()) {
    auto keyIt = dict.find(keyToUse);
    if (keyIt != dict.end())
      sharedMode = keyIt->second.mode;
  }
  if (sharedPath.empty()) {
    auto keyIt = dict.find(keyToUse);
    if (keyIt != dict.end())
      sharedPath = keyIt->second.path;
  }

  for (auto &[entryType, entry] : dict) {
    if (entryType == keyToUse)
      continue;
    if (!EntriesShareFixtureFamily(entry, sharedPath, sharedMode))
      continue;
    entry.visualColorHex = color;
  }
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

size_t GetSaveCallCountForTesting() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  return g_saveCallCountForTesting;
}

void ResetSaveCallCountForTesting() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  g_saveCallCountForTesting = 0;
}

} // namespace GdtfDictionary
