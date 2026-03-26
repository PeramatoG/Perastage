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

#include "dictionary_json_contract.h"
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

static fs::path GetDictFile() {
  fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("trusses"));
  if (dir.empty())
    return {};
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec)
    return {};
  fs::path file = dir / "truss_dictionary.json";
  if (!fs::exists(file)) {
    fs::path baseLib = ProjectUtils::GetBaseLibraryPath("trusses");
    fs::path baseFile = baseLib / "truss_dictionary.json";
    std::error_code ec;
    if (fs::exists(baseFile))
      fs::copy_file(baseFile, file, fs::copy_options::overwrite_existing, ec);
    if (!fs::exists(file)) {
      std::ofstream create(file);
      if (create.is_open())
        create << DictionaryJsonContract::MakeRoot("trusses", nlohmann::json::object()).dump(4);
    }
  }
  return file;
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
  fs::path src = fs::u8path(inputPath);
  if (!fs::exists(src)) {
    error = "Truss file does not exist";
    return false;
  }

  fs::path dictFile = GetDictFile();
  if (dictFile.empty()) {
    error = "Truss dictionary is not available";
    return false;
  }

  fs::path dir = dictFile.parent_path();
  fs::path working = src;
  if (!EnsureMigratedToGdtf(working, error))
    return false;

  fs::path dest = dir / working.filename();
  std::error_code ec;
  fs::copy_file(working, dest, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    error = "Failed to copy truss file into library";
    return false;
  }

  storedPath = ToUtf8String(dest);
  return true;
}

std::optional<std::unordered_map<std::string, std::string>> Load() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  std::unordered_map<std::string, std::string> dict;
  fs::path file = GetDictFile();
  if (file.empty())
    return std::nullopt;

  std::ifstream in(file);
  if (!in.is_open())
    return std::nullopt;

  if (in.peek() == std::ifstream::traits_type::eof()) {
    std::ofstream out(file);
    if (out.is_open())
      out << DictionaryJsonContract::MakeRoot("trusses", nlohmann::json::object()).dump(4);
    return dict;
  }

  nlohmann::json root;
  try {
    in >> root;
  } catch (const std::exception &ex) {
    std::cerr << "Invalid truss dictionary JSON in '" << file.string()
              << "': parse error: " << ex.what() << std::endl;
    return std::nullopt;
  }

  std::string contractError;
  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(
      root, "trusses", contractError);
  if (!entriesOpt) {
    std::cerr << "Invalid truss dictionary JSON in '" << file.string()
              << "': " << contractError << std::endl;
    return std::nullopt;
  }

  const nlohmann::json &entries = **entriesOpt;
  fs::path dir = file.parent_path();
  bool changed = false;

  auto resolveEntryPath = [&dir](const nlohmann::json &value, fs::path &entryPath,
                                 std::string &entryError) -> bool {
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

    entryPath = fs::u8path(pathValue);
    if (!entryPath.is_absolute())
      entryPath = dir / entryPath;
    return true;
  };

  auto processEntry = [&](const std::string &rawKey, const nlohmann::json &value,
                          const std::string &sourceLabel) -> bool {
    const std::string normalizedKey = NormalizeModelKey(rawKey);
    if (normalizedKey.empty()) {
      std::cerr << "Invalid truss dictionary JSON in '" << file.string()
                << "': " << sourceLabel << " has an empty model name" << std::endl;
      return false;
    }

    fs::path path;
    std::string entryError;
    if (!resolveEntryPath(value, path, entryError)) {
      std::cerr << "Invalid truss dictionary JSON in '" << file.string()
                << "': " << sourceLabel << " " << entryError << std::endl;
      return false;
    }

    if (!fs::exists(path))
      return true;

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
        std::cerr << "Invalid truss dictionary JSON in '" << file.string()
                  << "': entries[" << idx << "] must be an object" << std::endl;
        return std::nullopt;
      }
      if (!entryJson.contains("name") || !entryJson["name"].is_string()) {
        std::cerr << "Invalid truss dictionary JSON in '" << file.string()
                  << "': entries[" << idx << "] missing string 'name'" << std::endl;
        return std::nullopt;
      }
      if (!processEntry(entryJson["name"].get<std::string>(), entryJson,
                        "entries[" + std::to_string(idx) + "]")) {
        return std::nullopt;
      }
    }
  }

  if (changed)
    Save(dict);
  return dict;
}

void Save(const std::unordered_map<std::string, std::string> &dict) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  fs::path file = GetDictFile();
  if (file.empty())
    return;

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

  for (const auto &model : keys) {
    fs::path p = fs::u8path(normalizedDict.at(model));
    fs::path forced = p;
    if (forced.extension() != ".gdtf")
      forced.replace_extension(".gdtf");
    entries[model] = forced.filename().string();
  }

  const nlohmann::json root = DictionaryJsonContract::MakeRoot("trusses", std::move(entries));
  std::ofstream out(file);
  if (out.is_open())
    out << root.dump(4);
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

  if (!fs::exists(fs::u8path(it->second))) {
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

} // namespace TrussDictionary
