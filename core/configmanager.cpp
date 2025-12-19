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
#include "configmanager.h"
#include "../external/json.hpp"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <charconv>
#include <set>
#include <sstream>
#include <string_view>
#include <cctype>
#include <algorithm>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/stdpaths.h>
#include <wx/zipstrm.h>

namespace {
bool TryParseFloat(const std::string &text, float &out);

class TempDir {
public:
  explicit TempDir(const std::string &prefix) {
    namespace fs = std::filesystem;
    auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    path = fs::temp_directory_path() / (prefix + std::to_string(stamp));
    created = std::filesystem::create_directory(path);
  }

  TempDir(const TempDir &) = delete;
  TempDir &operator=(const TempDir &) = delete;

  TempDir(TempDir &&other) noexcept
      : path(std::move(other.path)), created(other.created) {
    other.created = false;
  }

  TempDir &operator=(TempDir &&other) noexcept {
    if (this != &other) {
      Cleanup();
      path = std::move(other.path);
      created = other.created;
      other.created = false;
    }
    return *this;
  }

  ~TempDir() { Cleanup(); }

  bool Valid() const { return created; }
  const std::filesystem::path &Path() const { return path; }

private:
  void Cleanup() {
    if (created) {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
      created = false;
    }
  }

  std::filesystem::path path;
  bool created = false;
};
} // namespace

ConfigManager::RevisionGuard::RevisionGuard(ConfigManager &cfg)
    : cfg(cfg), previous(cfg.suppressRevision) {
  cfg.suppressRevision = true;
}

ConfigManager::RevisionGuard::~RevisionGuard() {
  cfg.suppressRevision = previous;
}

static std::vector<std::string> SplitCSV(const std::string &s) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    size_t start = item.find_first_not_of(" \t");
    size_t end = item.find_last_not_of(" \t");
    if (start != std::string::npos)
      result.push_back(item.substr(start, end - start + 1));
  }
  return result;
}

static std::string JoinCSV(const std::vector<std::string> &items) {
  std::string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      out += ',';
    out += items[i];
  }
  return out;
}

ConfigManager::ConfigManager() {
  RevisionGuard guard(*this);
  RegisterVariable("camera_yaw", "float", 0.0f, -180.0f, 180.0f);
  RegisterVariable("camera_pitch", "float", 20.0f, -89.0f, 89.0f);
  RegisterVariable("camera_distance", "float", 30.0f, 0.5f, 500.0f);
  RegisterVariable("camera_target_x", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("camera_target_y", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("camera_target_z", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("rider_lx1_height", "float", 10.0f, 0.0f, 100.0f);
  RegisterVariable("rider_lx2_height", "float", 9.5f, 0.0f, 100.0f);
  RegisterVariable("rider_lx3_height", "float", 9.0f, 0.0f, 100.0f);
  RegisterVariable("rider_lx4_height", "float", 8.5f, 0.0f, 100.0f);
  RegisterVariable("rider_lx5_height", "float", 9.0f, 0.0f, 100.0f);
  RegisterVariable("rider_lx6_height", "float", 8.5f, 0.0f, 100.0f);
  RegisterVariable("rider_lx1_pos", "float", -2.0f, -100.0f, 100.0f);
  RegisterVariable("rider_lx2_pos", "float", 2.0f, -100.0f, 100.0f);
  RegisterVariable("rider_lx3_pos", "float", 4.0f, -100.0f, 100.0f);
  RegisterVariable("rider_lx4_pos", "float", 6.0f, -100.0f, 100.0f);
  RegisterVariable("rider_lx5_pos", "float", 8.0f, -100.0f, 100.0f);
  RegisterVariable("rider_lx6_pos", "float", 10.0f, -100.0f, 100.0f);
  RegisterVariable("rider_lx1_margin", "float", 0.2f, 0.0f, 10.0f);
  RegisterVariable("rider_lx2_margin", "float", 0.2f, 0.0f, 10.0f);
  RegisterVariable("rider_lx3_margin", "float", 0.2f, 0.0f, 10.0f);
  RegisterVariable("rider_lx4_margin", "float", 0.2f, 0.0f, 10.0f);
  RegisterVariable("rider_lx5_margin", "float", 0.2f, 0.0f, 10.0f);
  RegisterVariable("rider_lx6_margin", "float", 0.2f, 0.0f, 10.0f);
  // Grid rendering options
  RegisterVariable("grid_show", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("grid_style", "float", 0.0f, 0.0f, 2.0f);
  RegisterVariable("grid_color_r", "float", 0.35f, 0.0f, 1.0f);
  RegisterVariable("grid_color_g", "float", 0.35f, 0.0f, 1.0f);
  RegisterVariable("grid_color_b", "float", 0.35f, 0.0f, 1.0f);
  RegisterVariable("grid_draw_above", "float", 0.0f, 0.0f, 1.0f);
  RegisterVariable("print_include_grid", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("print_plan_page_size", "float", 0.0f, 0.0f, 1.0f,
                   {"print_page_size"});
  RegisterVariable("print_plan_landscape", "float", 0.0f, 0.0f, 1.0f,
                   {"print_landscape"});
  RegisterVariable("print_use_simplified_footprints", "float", 0.0f, 0.0f,
                   1.0f, {"use_simplified_footprints"});
  RegisterVariable("label_show_name", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_id", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_dmx", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_name_top", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_name_front", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_name_side", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_id_top", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_id_front", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_id_side", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_dmx_top", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_dmx_front", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_show_dmx_side", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("label_font_size_name", "float", 3.0f, 1.0f, 5.0f);
  RegisterVariable("label_font_size_id", "float", 2.0f, 1.0f, 5.0f);
  RegisterVariable("label_font_size_dmx", "float", 4.0f, 1.0f, 5.0f);
  RegisterVariable("label_offset_distance_top", "float", 0.5f, 0.0f, 1.0f);
  RegisterVariable("label_offset_angle_top", "float", 180.0f, 0.0f, 360.0f);
  RegisterVariable("label_offset_distance_front", "float", 0.5f, 0.0f, 1.0f);
  RegisterVariable("label_offset_angle_front", "float", 180.0f, 0.0f, 360.0f);
  RegisterVariable("label_offset_distance_side", "float", 0.5f, 0.0f, 1.0f);
  RegisterVariable("label_offset_angle_side", "float", 180.0f, 0.0f, 360.0f);
  RegisterVariable("view2d_offset_x", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("view2d_offset_y", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("view2d_zoom", "float", 1.0f, 0.1f, 100.0f);
  RegisterVariable("view2d_render_mode", "float", 2.0f, 0.0f, 3.0f);
  RegisterVariable("view2d_view", "float", 0.0f, 0.0f, 2.0f);
  RegisterVariable("view2d_dark_mode", "float", 1.0f, 0.0f, 1.0f);
  LoadUserConfig();
  if (!HasKey("rider_autopatch"))
    SetValue("rider_autopatch", "1");
  if (!HasKey("rider_layer_mode"))
    SetValue("rider_layer_mode", "position");
  ApplyColumnDefaults();
  ApplyDefaults();
  currentLayer = DEFAULT_LAYER_NAME;
}

ConfigManager &ConfigManager::Get() {
  static ConfigManager instance;
  return instance;
}

// -- Config key-value access --

void ConfigManager::SetValue(const std::string &key, const std::string &value) {
  std::string newValue = value;

  auto var = variables.find(key);
  if (var != variables.end() && var->second.type == "float") {
    float parsed = 0.0f;
    if (TryParseFloat(value, parsed)) {
      parsed = std::clamp(parsed, var->second.minValue, var->second.maxValue);
      var->second.value = parsed;
      newValue = std::to_string(parsed);
    }
  }

  auto it = configData.find(key);
  if (it != configData.end() && it->second == newValue)
    return;

  configData[key] = newValue;
  if (!suppressRevision)
    ++revision;
}

std::optional<std::string>
ConfigManager::GetValue(const std::string &key) const {
  auto it = configData.find(key);
  if (it != configData.end())
    return it->second;
  return std::nullopt;
}

namespace {
bool TryParseFloat(const std::string &text, float &out) {
  // Avoid throwing exceptions on malformed values by using from_chars.
  // This keeps hot paths free of costly exception handling when config
  // entries are empty or contain unexpected characters.
  if (text.empty())
    return false;

  // Allow leading and trailing spaces that may appear in user edited files.
  const auto first =
      std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
      });
  if (first == text.end())
    return false;
  const auto last =
      std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  std::string_view trimmed(&(*first), static_cast<size_t>(last - first));

  auto *begin = trimmed.data();
  auto *end = trimmed.data() + trimmed.size();

  auto result = std::from_chars(begin, end, out);
  // Success requires the conversion to finish without errors and consume the
  // entire trimmed token so trailing garbage is rejected.
  return result.ec == std::errc{} && result.ptr == end;
}
} // namespace

bool ConfigManager::HasKey(const std::string &key) const {
  return configData.find(key) != configData.end();
}

void ConfigManager::RemoveKey(const std::string &key) {
  size_t erased = configData.erase(key);
  if (erased > 0 && !suppressRevision)
    ++revision;
}

void ConfigManager::ClearValues() {
  if (configData.empty())
    return;
  configData.clear();
  if (!suppressRevision)
    ++revision;
}

void ConfigManager::RegisterVariable(const std::string &name,
                                     const std::string &type, float defVal,
                                     float minVal, float maxVal,
                                     std::vector<std::string> legacyNames) {
  VariableInfo info;
  info.type = type;
  info.defaultValue = defVal;
  info.value = defVal;
  info.minValue = minVal;
  info.maxValue = maxVal;
  info.legacyNames = std::move(legacyNames);
  variables[name] = info;
}

float ConfigManager::GetFloat(const std::string &name) const {
  auto it = variables.find(name);
  float defVal = 0.0f;
  if (it != variables.end())
    defVal = it->second.defaultValue;

  auto valStr = GetValue(name);
  if (valStr) {
    float parsed = 0.0f;
    if (TryParseFloat(*valStr, parsed))
      return parsed;
    return defVal;
  }
  return defVal;
}

void ConfigManager::SetFloat(const std::string &name, float v) {
  auto it = variables.find(name);
  if (it != variables.end()) {
    v = std::clamp(v, it->second.minValue, it->second.maxValue);
    it->second.value = v;
  }
  SetValue(name, std::to_string(v));
}

void ConfigManager::ApplyDefaults() {
  for (auto &[name, info] : variables) {
    float val = info.defaultValue;
    const std::string *raw = nullptr;

    for (const auto &legacyName : info.legacyNames) {
      auto itLegacy = configData.find(legacyName);
      if (itLegacy != configData.end()) {
        raw = &itLegacy->second;
        break;
      }
    }

    if (!raw) {
      auto it = configData.find(name);
      if (it != configData.end())
        raw = &it->second;
    }

    if (raw) {
      float parsed = 0.0f;
      if (TryParseFloat(*raw, parsed))
        val = parsed;
    }

    val = std::clamp(val, info.minValue, info.maxValue);
    info.value = val;
    configData[name] = std::to_string(val);
  }
}

void ConfigManager::ApplyColumnDefaults() {
  if (!HasKey("fixture_print_columns"))
    SetValue(
        "fixture_print_columns",
        "Fixture ID,Name,Type,Layer,Hang Pos,Universe,Channel,Mode,Ch Count");
  if (!HasKey("truss_print_columns"))
    SetValue("truss_print_columns", "Name,Layer,Hang Pos,Manufacturer,Model");
  if (!HasKey("support_print_columns"))
    SetValue("support_print_columns",
             "Hoist ID,Name,Type,Function,Layer,Hang Pos,Pos X,Pos Y,Pos Z,Roll (X),Pitch (Y),Yaw (Z),Chain Length (m),Capacity (kg),Weight (kg)");
  if (!HasKey("sceneobject_print_columns"))
    SetValue("sceneobject_print_columns", "Name,Layer");
}

std::vector<std::string> ConfigManager::GetFixturePrintColumns() const {
  auto val = GetValue("fixture_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void ConfigManager::SetFixturePrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("fixture_print_columns", JoinCSV(cols));
}

std::vector<std::string> ConfigManager::GetTrussPrintColumns() const {
  auto val = GetValue("truss_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void ConfigManager::SetTrussPrintColumns(const std::vector<std::string> &cols) {
  SetValue("truss_print_columns", JoinCSV(cols));
}

std::vector<std::string> ConfigManager::GetSupportPrintColumns() const {
  auto val = GetValue("support_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void ConfigManager::SetSupportPrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("support_print_columns", JoinCSV(cols));
}

std::vector<std::string> ConfigManager::GetSceneObjectPrintColumns() const {
  auto val = GetValue("sceneobject_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void ConfigManager::SetSceneObjectPrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("sceneobject_print_columns", JoinCSV(cols));
}

std::unordered_set<std::string> ConfigManager::GetHiddenLayers() const {
  std::unordered_set<std::string> out;
  auto val = GetValue("hidden_layers");
  if (val) {
    for (const auto &s : SplitCSV(*val))
      out.insert(s);
  }
  return out;
}

void ConfigManager::SetHiddenLayers(
    const std::unordered_set<std::string> &layers) {
  std::vector<std::string> v(layers.begin(), layers.end());
  SetValue("hidden_layers", JoinCSV(v));
}

bool ConfigManager::IsLayerVisible(const std::string &layer) const {
  std::string name = layer.empty() ? DEFAULT_LAYER_NAME : layer;
  auto hidden = GetHiddenLayers();
  return hidden.find(name) == hidden.end();
}

void ConfigManager::SetLayerColor(const std::string &layer,
                                  const std::string &color) {
  std::string name = layer.empty() ? DEFAULT_LAYER_NAME : layer;
  std::string layerUuid;
  for (auto &[uuid, l] : scene.layers) {
    if (l.name == name) {
      layerUuid = uuid;
      break;
    }
  }
  if (layerUuid.empty()) {
    Layer l;
    l.name = name;
    l.color = color;
    l.uuid = "layer_" + std::to_string(scene.layers.size() + 1);
    scene.layers[l.uuid] = l;
  } else {
    scene.layers[layerUuid].color = color;
  }
}

std::optional<std::string>
ConfigManager::GetLayerColor(const std::string &layer) const {
  std::string name = layer.empty() ? DEFAULT_LAYER_NAME : layer;
  for (const auto &[uuid, l] : scene.layers) {
    if (l.name == name && !l.color.empty())
      return l.color;
  }
  return std::nullopt;
}

std::vector<std::string> ConfigManager::GetLayerNames() const {
  std::set<std::string> names;
  for (const auto &[uuid, layer] : scene.layers)
    names.insert(layer.name);
  auto collect = [&](const std::string &ln) {
    if (!ln.empty())
      names.insert(ln);
  };
  for (const auto &[u, f] : scene.fixtures)
    collect(f.layer);
  for (const auto &[u, t] : scene.trusses)
    collect(t.layer);
  for (const auto &[u, s] : scene.supports)
    collect(s.layer);
  for (const auto &[u, o] : scene.sceneObjects)
    collect(o.layer);
  names.insert(DEFAULT_LAYER_NAME);
  return {names.begin(), names.end()};
}

const std::string &ConfigManager::GetCurrentLayer() const {
  return currentLayer;
}

void ConfigManager::SetCurrentLayer(const std::string &name) {
  if (name.empty())
    currentLayer = DEFAULT_LAYER_NAME;
  else
    currentLayer = name;
}

// -- Scene access --

MvrScene &ConfigManager::GetScene() { return scene; }

const MvrScene &ConfigManager::GetScene() const { return scene; }

const std::vector<std::string> &ConfigManager::GetSelectedFixtures() const {
  return selectedFixtures;
}

void ConfigManager::SetSelectedFixtures(const std::vector<std::string> &uuids) {
  selectedFixtures = uuids;
}

const std::vector<std::string> &ConfigManager::GetSelectedTrusses() const {
  return selectedTrusses;
}

void ConfigManager::SetSelectedTrusses(const std::vector<std::string> &uuids) {
  selectedTrusses = uuids;
}

const std::vector<std::string> &ConfigManager::GetSelectedSupports() const {
  return selectedSupports;
}

void ConfigManager::SetSelectedSupports(const std::vector<std::string> &uuids) {
  selectedSupports = uuids;
}

const std::vector<std::string> &ConfigManager::GetSelectedSceneObjects() const {
  return selectedSceneObjects;
}

void ConfigManager::SetSelectedSceneObjects(
    const std::vector<std::string> &uuids) {
  selectedSceneObjects = uuids;
}

// -- Persistence --

bool ConfigManager::LoadFromFile(const std::string &path) {
  RevisionGuard guard(*this);
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }
  if (!j.is_object())
    return false;

  try {
    configData = j.get<std::unordered_map<std::string, std::string>>();
  } catch (...) {
    return false;
  }
  ApplyColumnDefaults();
  ApplyDefaults();
  return true;
}

bool ConfigManager::SaveToFile(const std::string &path) const {
  std::ofstream file(path);
  if (!file.is_open())
    return false;

  nlohmann::json j(configData);
  file << j.dump(4);
  return true;
}

bool ConfigManager::SaveProject(const std::string &path) {
  namespace fs = std::filesystem;
  TempDir tempDir("PerastageProj_");
  if (!tempDir.Valid())
    return false;

  fs::path configPath = tempDir.Path() / "config.json";
  fs::path scenePath = tempDir.Path() / "scene.mvr";

  if (!SaveToFile(configPath.string())) {
    return false;
  }

  MvrExporter exporter;
  if (!exporter.ExportToFile(scenePath.string())) {
    return false;
  }

  wxFileOutputStream out(path);
  if (!out.IsOk()) {
    return false;
  }

  wxZipOutputStream zip(out);

  auto addFile = [&](const fs::path &p, const std::string &name) {
    auto *entry = new wxZipEntry(name);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    std::ifstream in(p, std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  };

  addFile(configPath, "config.json");
  addFile(scenePath, "scene.mvr");

  zip.Close();
  savedRevision = revision;
  return true;
}

bool ConfigManager::LoadProject(const std::string &path) {
  namespace fs = std::filesystem;

  wxFileInputStream in(path);
  if (!in.IsOk())
    return false;

  wxZipInputStream zip(in);
  std::unique_ptr<wxZipEntry> entry;

  TempDir tempDir("PerastageProj_");
  if (!tempDir.Valid())
    return false;

  fs::path configPath;
  fs::path scenePath;

  while ((entry.reset(zip.GetNextEntry())), entry) {
    std::string name = entry->GetName().ToStdString();
    fs::path outPath;
    if (name == "config.json")
      outPath = tempDir.Path() / "config.json";
    else if (name == "scene.mvr")
      outPath = tempDir.Path() / "scene.mvr";
    else
      continue;

    std::ofstream out(outPath, std::ios::binary);
    char buf[4096];
    while (true) {
      zip.Read(buf, sizeof(buf));
      size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      out.write(buf, bytes);
    }
    out.close();

    if (name == "config.json")
      configPath = outPath;
    else if (name == "scene.mvr")
      scenePath = outPath;
  }

  bool ok = true;
  if (!scenePath.empty())
    ok &= MvrImporter::ImportAndRegister(scenePath.string(), false);
  if (!configPath.empty())
    ok &= LoadFromFile(configPath.string());

  if (ok) {
    ClearHistory();
    selectedFixtures.clear();
    selectedTrusses.clear();
    selectedSupports.clear();
    selectedSceneObjects.clear();
    revision = 0;
    savedRevision = 0;
  }
  return ok;
}

void ConfigManager::Reset() {
  RevisionGuard guard(*this);
  configData.clear();
  scene.Clear();
  if (!HasKey("rider_autopatch"))
    SetValue("rider_autopatch", "1");
  ApplyDefaults();
  selectedFixtures.clear();
  selectedTrusses.clear();
  selectedSupports.clear();
  selectedSceneObjects.clear();
  currentLayer = DEFAULT_LAYER_NAME;
  ClearHistory();
  revision = 0;
  savedRevision = 0;
}

std::string ConfigManager::GetUserConfigFile() {
  wxString dir = wxStandardPaths::Get().GetUserDataDir();
  std::filesystem::path p = std::filesystem::path(dir.ToStdString());
  std::filesystem::create_directories(p);
  p /= "user_config.json";
  return p.string();
}

bool ConfigManager::LoadUserConfig() {
  return LoadFromFile(GetUserConfigFile());
}

bool ConfigManager::SaveUserConfig() const {
  return SaveToFile(GetUserConfigFile());
}

void ConfigManager::PushUndoState(const std::string &description) {
  Snapshot snap{scene, selectedFixtures, selectedTrusses, selectedSupports,
                selectedSceneObjects, description};
  undoStack.push_back(std::move(snap));
  if (undoStack.size() > maxHistory)
    undoStack.erase(undoStack.begin());
  redoStack.clear();
  ++revision;
}

bool ConfigManager::CanUndo() const { return !undoStack.empty(); }

bool ConfigManager::CanRedo() const { return !redoStack.empty(); }

std::string ConfigManager::Undo() {
  if (undoStack.empty())
    return {};
  const Snapshot snap = undoStack.back();
  redoStack.push_back({scene, selectedFixtures, selectedTrusses,
                       selectedSupports, selectedSceneObjects,
                       snap.description});
  scene = snap.scene;
  selectedFixtures = snap.selFixtures;
  selectedTrusses = snap.selTrusses;
  selectedSupports = snap.selSupports;
  selectedSceneObjects = snap.selSceneObjects;
  undoStack.pop_back();
  if (revision > 0)
    --revision;
  return snap.description;
}

std::string ConfigManager::Redo() {
  if (redoStack.empty())
    return {};
  const Snapshot snap = redoStack.back();
  undoStack.push_back({scene, selectedFixtures, selectedTrusses,
                       selectedSupports, selectedSceneObjects,
                       snap.description});
  scene = snap.scene;
  selectedFixtures = snap.selFixtures;
  selectedTrusses = snap.selTrusses;
  selectedSupports = snap.selSupports;
  selectedSceneObjects = snap.selSceneObjects;
  redoStack.pop_back();
  ++revision;
  return snap.description;
}

void ConfigManager::ClearHistory() {
  undoStack.clear();
  redoStack.clear();
}

bool ConfigManager::IsDirty() const { return revision != savedRevision; }

void ConfigManager::MarkSaved() { savedRevision = revision; }
