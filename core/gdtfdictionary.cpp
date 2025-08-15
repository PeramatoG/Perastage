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
#include "../external/json.hpp"
#include "projectutils.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace GdtfDictionary {

static fs::path GetDictFile() {
  fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  if (dir.empty())
    return {};
  fs::create_directories(dir);
  fs::path file = dir / "gdtf_dictionary.json";
  if (!fs::exists(file)) {
    std::ofstream create(file);
    if (create.is_open())
      create << "{}";
  }
  return file;
}

std::optional<std::unordered_map<std::string, Entry>> Load() {
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
      out << "{}";
    return dict;
  }
  nlohmann::json j;
  try {
    in >> j;
  } catch (...) {
    std::ofstream out(file);
    if (out.is_open())
      out << "{}";
    return dict;
  }
  if (!j.is_object()) {
    std::ofstream out(file);
    if (out.is_open())
      out << "{}";
    return dict;
  }
  fs::path dir = file.parent_path();
  for (auto it = j.begin(); it != j.end(); ++it) {
    if (it.value().is_string()) {
      fs::path p = fs::u8path(it.value().get<std::string>());
      if (!p.is_absolute())
        p = dir / p;
      dict[it.key()] = {p.string(), ""};
    } else if (it.value().is_object()) {
      Entry e;
      std::string fname;
      if (it.value().contains("file") && it.value()["file"].is_string())
        fname = it.value()["file"].get<std::string>();
      else if (it.value().contains("path") && it.value()["path"].is_string())
        fname = it.value()["path"].get<std::string>();
      fs::path p = fs::u8path(fname);
      if (!p.is_absolute())
        p = dir / p;
      e.path = p.string();
      if (it.value().contains("mode") && it.value()["mode"].is_string())
        e.mode = it.value()["mode"].get<std::string>();
      dict[it.key()] = e;
    }
  }
  return dict;
}

void Save(const std::unordered_map<std::string, Entry> &dict) {
  fs::path file = GetDictFile();
  if (file.empty())
    return;
  nlohmann::json j;
  for (const auto &[type, entry] : dict) {
    nlohmann::json obj;
    fs::path p = fs::u8path(entry.path);
    obj["file"] = p.filename().string();
    if (!entry.mode.empty())
      obj["mode"] = entry.mode;
    j[type] = obj;
  }
  std::ofstream out(file);
  if (!out.is_open())
    return;
  out << j.dump(4);
}

std::optional<Entry> Get(const std::string &type) {
  auto dictOpt = Load();
  if (!dictOpt)
    return std::nullopt;
  auto &dict = *dictOpt;
  auto it = dict.find(type);
  if (it == dict.end())
    return std::nullopt;
  if (!fs::exists(it->second.path)) {
    dict.erase(it);
    Save(dict);
    return std::nullopt;
  }
  return it->second;
}

void Update(const std::string &type, const std::string &gdtfPath, const std::string &mode) {
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
  dict[type] = e;
  Save(dict);
}

} // namespace GdtfDictionary
