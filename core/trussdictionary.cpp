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
#include "../external/json.hpp"
#include "projectutils.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace TrussDictionary {

static fs::path GetDictFile() {
  fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("trusses"));
  if (dir.empty())
    return {};
  fs::create_directories(dir);
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
        create << "{}";
    }
  }
  return file;
}

std::optional<std::unordered_map<std::string, std::string>> Load() {
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
      dict[it.key()] = p.string();
    }
  }
  return dict;
}

void Save(const std::unordered_map<std::string, std::string> &dict) {
  fs::path file = GetDictFile();
  if (file.empty())
    return;
  nlohmann::json j;
  for (const auto &[model, path] : dict) {
    fs::path p = fs::u8path(path);
    j[model] = p.filename().string();
  }
  std::ofstream out(file);
  if (!out.is_open())
    return;
  out << j.dump(4);
}

std::optional<std::string> Get(const std::string &model) {
  auto dictOpt = Load();
  if (!dictOpt)
    return std::nullopt;
  auto &dict = *dictOpt;
  auto it = dict.find(model);
  if (it == dict.end())
    return std::nullopt;
  if (!fs::exists(it->second)) {
    dict.erase(it);
    Save(dict);
    return std::nullopt;
  }
  return it->second;
}

void Update(const std::string &model, const std::string &modelPath) {
  if (model.empty() || modelPath.empty())
    return;
  fs::path src = fs::u8path(modelPath);
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
    return;
  auto &dict = *dictOpt;
  dict[model] = dest.string();
  Save(dict);
}

} // namespace TrussDictionary

