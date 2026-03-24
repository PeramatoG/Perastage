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
#include "json.hpp"
#include "logger.h"
#include "projectutils.h"
#include "startup_file_access_gate.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;

namespace GdtfDictionary {

namespace {
std::mutex g_issueMutex;
std::optional<AccessIssue> g_lastAccessIssue;

std::string ErrnoMessage() {
  const int errorValue = errno;
  if (errorValue == 0)
    return "unknown filesystem error";
  return std::string(std::strerror(errorValue));
}

void SetLastAccessIssue(const fs::path &path, const std::string &operation,
                        const std::string &cause) {
  AccessIssue issue{path.string(), operation, cause};
  {
    std::lock_guard<std::mutex> lock(g_issueMutex);
    g_lastAccessIssue = issue;
  }
  Logger::Instance().Log(
      Logger::Level::Error,
      "GdtfDictionary file access failed (" + operation + ") at '" +
          issue.attemptedPath + "': " + issue.cause);
}

void ClearLastAccessIssue() {
  std::lock_guard<std::mutex> lock(g_issueMutex);
  g_lastAccessIssue.reset();
}
} // namespace

static fs::path GetDictFile() {
  fs::path dir = fs::u8path(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  if (dir.empty()) {
    SetLastAccessIssue(dir, "resolve directory",
                       "default fixtures library path is empty");
    return {};
  }
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    SetLastAccessIssue(dir, "create directories", ec.message());
    return {};
  }
  fs::path file = dir / "gdtf_dictionary.json";
  if (!fs::exists(file)) {
    fs::path baseLib = ProjectUtils::GetBaseLibraryPath("fixtures");
    fs::path baseFile = baseLib / "gdtf_dictionary.json";
    std::error_code ec;
    if (fs::exists(baseFile))
      fs::copy_file(baseFile, file, fs::copy_options::overwrite_existing, ec);
    if (ec) {
      SetLastAccessIssue(file, "copy base dictionary", ec.message());
      return {};
    }
    if (!fs::exists(file)) {
      std::ofstream create(file);
      if (create.is_open()) {
        create << "{}";
      } else {
        SetLastAccessIssue(file, "create file", ErrnoMessage());
        return {};
      }
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
  if (!in.is_open()) {
    SetLastAccessIssue(file, "open for read", ErrnoMessage());
    return std::nullopt;
  }
  if (in.peek() == std::ifstream::traits_type::eof()) {
    std::ofstream out(file);
    if (out.is_open()) {
      out << "{}";
    } else {
      SetLastAccessIssue(file, "open for write", ErrnoMessage());
    }
    return dict;
  }
  nlohmann::json j;
  try {
    in >> j;
  } catch (...) {
    std::ofstream out(file);
    if (out.is_open()) {
      out << "{}";
    } else {
      SetLastAccessIssue(file, "rewrite invalid json", ErrnoMessage());
    }
    return dict;
  }
  if (!j.is_object()) {
    std::ofstream out(file);
    if (out.is_open()) {
      out << "{}";
    } else {
      SetLastAccessIssue(file, "rewrite non-object json", ErrnoMessage());
    }
    return dict;
  }
  fs::path dir = file.parent_path();
  for (auto it = j.begin(); it != j.end(); ++it) {
    if (it.value().is_string()) {
      fs::path p = fs::u8path(it.value().get<std::string>());
      if (!p.is_absolute())
        p = dir / p;
      dict[it.key()] = {p.string(), "", ""};
    } else if (it.value().is_object()) {
      Entry e;
      std::string fname;
      if (it.value().contains("file") && it.value()["file"].is_string())
        fname = it.value()["file"].get<std::string>();
      else if (it.value().contains("path") && it.value()["path"].is_string())
        fname = it.value()["path"].get<std::string>();
      if (fname.empty())
        continue;
      fs::path p = fs::u8path(fname);
      if (!p.is_absolute())
        p = dir / p;
      e.path = p.string();
      if (it.value().contains("mode") && it.value()["mode"].is_string())
        e.mode = it.value()["mode"].get<std::string>();
      if (it.value().contains("category") && it.value()["category"].is_string())
        e.category = it.value()["category"].get<std::string>();
      dict[it.key()] = e;
    }
  }
  ClearLastAccessIssue();
  return dict;
}

void Save(const std::unordered_map<std::string, Entry> &dict) {
  std::lock_guard<std::recursive_mutex> lock(StartupFileAccessGate::Mutex());
  fs::path file = GetDictFile();
  if (file.empty())
    return;
  nlohmann::json j;
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (const auto &[type, entry] : dict)
    keys.push_back(type);
  std::sort(keys.begin(), keys.end());
  for (const auto &type : keys) {
    const auto &entry = dict.at(type);
    if (entry.path.empty())
      continue;
    nlohmann::json obj;
    fs::path p = fs::u8path(entry.path);
    const std::string fileName = p.filename().string();
    if (fileName.empty())
      continue;
    obj["file"] = fileName;
    if (!entry.mode.empty())
      obj["mode"] = entry.mode;
    if (!entry.category.empty())
      obj["category"] = entry.category;
    j[type] = obj;
  }
  std::ofstream out(file);
  if (!out.is_open()) {
    SetLastAccessIssue(file, "open for write", ErrnoMessage());
    return;
  }
  out << j.dump(4);
  ClearLastAccessIssue();
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
  if (!fs::exists(it->second.path)) {
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
  if (it == dict.end())
    return;
  it->second.category = category;
  Save(dict);
}

std::optional<AccessIssue> ConsumeLastAccessIssue() {
  std::lock_guard<std::mutex> lock(g_issueMutex);
  std::optional<AccessIssue> issue = g_lastAccessIssue;
  g_lastAccessIssue.reset();
  return issue;
}

} // namespace GdtfDictionary
