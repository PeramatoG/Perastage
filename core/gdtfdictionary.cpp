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
  for (auto it = j.begin(); it != j.end(); ++it) {
    if (it.value().is_string()) {
      dict[it.key()] = {it.value().get<std::string>(), ""};
    } else if (it.value().is_object()) {
      Entry e;
      if (it.value().contains("path") && it.value()["path"].is_string())
        e.path = it.value()["path"].get<std::string>();
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
    obj["path"] = entry.path;
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
