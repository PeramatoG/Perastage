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

#include "json.hpp"
#include "projectutils.h"
#include "truss_gdtf_builder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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

  error = "Unsupported truss format";
  return false;
}

} // namespace

bool ImportTrussFile(const std::string &inputPath, std::string &storedPath,
                     std::string &error) {
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
  bool changed = false;

  for (auto it = j.begin(); it != j.end(); ++it) {
    if (!it.value().is_string())
      continue;

    fs::path path = fs::u8path(it.value().get<std::string>());
    if (!path.is_absolute())
      path = dir / path;
    if (!fs::exists(path))
      continue;

    std::string error;
    fs::path migrated = path;
    if (!EnsureMigratedToGdtf(migrated, error))
      continue;

    if (migrated != path)
      changed = true;
    dict[it.key()] = ToUtf8String(migrated);
  }

  if (changed)
    Save(dict);
  return dict;
}

void Save(const std::unordered_map<std::string, std::string> &dict) {
  fs::path file = GetDictFile();
  if (file.empty())
    return;

  nlohmann::json j;
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[model, _] : dict)
    keys.push_back(model);
  std::sort(keys.begin(), keys.end());

  for (const auto &model : keys) {
    fs::path p = fs::u8path(dict.at(model));
    fs::path forced = p;
    if (forced.extension() != ".gdtf")
      forced.replace_extension(".gdtf");
    j[model] = forced.filename().string();
  }

  std::ofstream out(file);
  if (out.is_open())
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

  if (!fs::exists(fs::u8path(it->second))) {
    dict.erase(it);
    Save(dict);
    return std::nullopt;
  }

  return it->second;
}

void Update(const std::string &model, const std::string &modelPath) {
  if (model.empty() || modelPath.empty())
    return;

  std::string storedPath;
  std::string error;
  if (!ImportTrussFile(modelPath, storedPath, error))
    return;

  auto dictOpt = Load();
  if (!dictOpt)
    return;

  auto &dict = *dictOpt;
  dict[model] = storedPath;
  Save(dict);
}

} // namespace TrussDictionary
