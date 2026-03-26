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

static fs::path GetDictFile() {
  fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  if (dir.empty())
    return {};
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec)
    return {};
  fs::path file = dir / "gdtf_dictionary.json";
  if (!fs::exists(file)) {
    fs::path baseLib = ProjectUtils::GetBaseLibraryPath("fixtures");
    fs::path baseFile = baseLib / "gdtf_dictionary.json";
    std::error_code ec;
    if (fs::exists(baseFile))
      fs::copy_file(baseFile, file, fs::copy_options::overwrite_existing, ec);
    if (!fs::exists(file)) {
      std::ofstream create(file);
      if (create.is_open())
        create << DictionaryJsonContract::MakeRoot("fixtures", nlohmann::json::object()).dump(4);
    }
  }
  return file;
}

std::optional<std::unordered_map<std::string, Entry>> Load() {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  std::unordered_map<std::string, Entry> dict;
  fs::path file = GetDictFile();
  if (file.empty())
    return std::nullopt;
  std::ifstream in(file);
  if (!in.is_open())
    return std::nullopt;
  if (in.peek() == std::ifstream::traits_type::eof()) {
    std::ofstream out(file);
    if (out.is_open())
      out << DictionaryJsonContract::MakeRoot("fixtures", nlohmann::json::object()).dump(4);
    return dict;
  }
  nlohmann::json root;
  try {
    in >> root;
  } catch (const std::exception &ex) {
    std::cerr << "Invalid fixtures dictionary JSON in '" << file.string()
              << "': parse error: " << ex.what() << std::endl;
    return std::nullopt;
  }

  std::string contractError;
  auto entriesOpt = DictionaryJsonContract::GetEntriesForType(
      root, "fixtures", contractError);
  if (!entriesOpt) {
    std::cerr << "Invalid fixtures dictionary JSON in '" << file.string()
              << "': " << contractError << std::endl;
    return std::nullopt;
  }

  const nlohmann::json &entries = **entriesOpt;
  fs::path dir = file.parent_path();
  auto parseEntryValue = [&dir](const nlohmann::json &value, Entry &entry,
                                std::string &entryError) -> bool {
    if (value.is_string()) {
      fs::path p = fs::u8path(value.get<std::string>());
      if (!p.is_absolute())
        p = dir / p;
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
      fs::path p = fs::u8path(fname);
      if (!p.is_absolute())
        p = dir / p;
      entry.path = p.string();
    }
    if (value.contains("mode") && value["mode"].is_string())
      entry.mode = value["mode"].get<std::string>();
    if (value.contains("category") && value["category"].is_string())
      entry.category = value["category"].get<std::string>();
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
      if (!parseEntryValue(it.value(), entry, entryError)) {
        std::cerr << "Invalid fixtures dictionary JSON in '" << file.string()
                  << "': invalid entry '" << it.key() << "': " << entryError
                  << std::endl;
        return std::nullopt;
      }
      dict[it.key()] = entry;
    }
    return dict;
  }

  for (size_t idx = 0; idx < entries.size(); ++idx) {
    const nlohmann::json &entryJson = entries[idx];
    if (!entryJson.is_object()) {
      std::cerr << "Invalid fixtures dictionary JSON in '" << file.string()
                << "': entries[" << idx << "] must be an object" << std::endl;
      return std::nullopt;
    }
    if (!entryJson.contains("name") || !entryJson["name"].is_string()) {
      std::cerr << "Invalid fixtures dictionary JSON in '" << file.string()
                << "': entries[" << idx << "] missing string 'name'" << std::endl;
      return std::nullopt;
    }

    Entry entry;
    std::string entryError;
    if (!parseEntryValue(entryJson, entry, entryError)) {
      std::cerr << "Invalid fixtures dictionary JSON in '" << file.string()
                << "': entries[" << idx << "] " << entryError << std::endl;
      return std::nullopt;
    }

    dict[entryJson["name"].get<std::string>()] = entry;
  }
  return dict;
}

void Save(const std::unordered_map<std::string, Entry> &dict) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  fs::path file = GetDictFile();
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
  fs::path src = fs::u8path(gdtfPath);
  if (!fs::exists(src))
    return;
  fs::path file = GetDictFile();
  if (file.empty())
    return;
  fs::path dir = file.parent_path();
  fs::path dest = dir / src.filename();
  try {
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
  } catch (...) {
    // ignore copy errors
  }
  auto dictOpt = Load();
  if (!dictOpt)
    return; // avoid overwriting existing dictionary on load failure
  auto &dict = *dictOpt;
  Entry e;
  auto it = dict.find(type);
  if (it != dict.end())
    e = it->second;
  e.path = dest.string();
  if (!mode.empty())
    e.mode = mode;
  if (!category.empty())
    e.category = category;
  dict[type] = e;
  Save(dict);
}


void UpdateCategory(const std::string &type, const std::string &category) {
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
    it->second.category = category;
  }
  Save(dict);
}

} // namespace GdtfDictionary
