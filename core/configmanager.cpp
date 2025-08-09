#include "configmanager.h"
#include "../external/json.hpp"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/stdpaths.h>
#include <wx/zipstrm.h>

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
  RegisterVariable("camera_yaw", "float", 0.0f, -180.0f, 180.0f);
  RegisterVariable("camera_pitch", "float", 20.0f, -89.0f, 89.0f);
  RegisterVariable("camera_distance", "float", 30.0f, 0.5f, 500.0f);
  RegisterVariable("camera_target_x", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("camera_target_y", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("camera_target_z", "float", 0.0f, -1000.0f, 1000.0f);
  RegisterVariable("rider_lx1_height", "float", 6.0f, 0.0f, 100.0f);
  RegisterVariable("rider_lx2_height", "float", 5.5f, 0.0f, 100.0f);
  RegisterVariable("rider_lx3_height", "float", 5.0f, 0.0f, 100.0f);
  RegisterVariable("rider_lx4_height", "float", 4.5f, 0.0f, 100.0f);
  RegisterVariable("rider_lx5_height", "float", 4.0f, 0.0f, 100.0f);
  RegisterVariable("rider_lx6_height", "float", 3.5f, 0.0f, 100.0f);
  LoadUserConfig();
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
  configData[key] = value;
}

std::optional<std::string>
ConfigManager::GetValue(const std::string &key) const {
  auto it = configData.find(key);
  if (it != configData.end())
    return it->second;
  return std::nullopt;
}

bool ConfigManager::HasKey(const std::string &key) const {
  return configData.find(key) != configData.end();
}

void ConfigManager::RemoveKey(const std::string &key) { configData.erase(key); }

void ConfigManager::ClearValues() { configData.clear(); }

void ConfigManager::RegisterVariable(const std::string &name,
                                     const std::string &type, float defVal,
                                     float minVal, float maxVal) {
  VariableInfo info;
  info.type = type;
  info.defaultValue = defVal;
  info.value = defVal;
  info.minValue = minVal;
  info.maxValue = maxVal;
  variables[name] = info;
}

float ConfigManager::GetFloat(const std::string &name) const {
  auto it = variables.find(name);
  float defVal = 0.0f;
  if (it != variables.end())
    defVal = it->second.defaultValue;

  auto valStr = GetValue(name);
  if (valStr) {
    try {
      return std::stof(*valStr);
    } catch (...) {
      return defVal;
    }
  }
  return defVal;
}

void ConfigManager::SetFloat(const std::string &name, float v) {
  auto it = variables.find(name);
  if (it != variables.end()) {
    if (v < it->second.minValue)
      v = it->second.minValue;
    if (v > it->second.maxValue)
      v = it->second.maxValue;
    it->second.value = v;
  }
  SetValue(name, std::to_string(v));
}

void ConfigManager::ApplyDefaults() {
  for (auto &[name, info] : variables) {
    float val = info.defaultValue;
    auto it = configData.find(name);
    if (it != configData.end()) {
      try {
        val = std::stof(it->second);
      } catch (...) {
        val = info.defaultValue;
      }
    }
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

const std::vector<std::string> &ConfigManager::GetSelectedSceneObjects() const {
  return selectedSceneObjects;
}

void ConfigManager::SetSelectedSceneObjects(
    const std::vector<std::string> &uuids) {
  selectedSceneObjects = uuids;
}

// -- Persistence --

bool ConfigManager::LoadFromFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }

  configData = j.get<std::unordered_map<std::string, std::string>>();
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
  fs::path tempDir =
      fs::temp_directory_path() /
      ("PerastageProj_" +
       std::to_string(
           std::chrono::system_clock::now().time_since_epoch().count()));
  fs::create_directory(tempDir);

  fs::path configPath = tempDir / "config.json";
  fs::path scenePath = tempDir / "scene.mvr";

  if (!SaveToFile(configPath.string())) {
    fs::remove_all(tempDir);
    return false;
  }

  MvrExporter exporter;
  if (!exporter.ExportToFile(scenePath.string())) {
    fs::remove_all(tempDir);
    return false;
  }

  wxFileOutputStream out(path);
  if (!out.IsOk()) {
    fs::remove_all(tempDir);
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
  fs::remove_all(tempDir);
  return true;
}

bool ConfigManager::LoadProject(const std::string &path) {
  namespace fs = std::filesystem;

  wxFileInputStream in(path);
  if (!in.IsOk())
    return false;

  wxZipInputStream zip(in);
  std::unique_ptr<wxZipEntry> entry;

  fs::path tempDir =
      fs::temp_directory_path() /
      ("PerastageProj_" +
       std::to_string(
           std::chrono::system_clock::now().time_since_epoch().count()));
  fs::create_directory(tempDir);

  fs::path configPath;
  fs::path scenePath;

  while ((entry.reset(zip.GetNextEntry())), entry) {
    std::string name = entry->GetName().ToStdString();
    fs::path outPath;
    if (name == "config.json")
      outPath = tempDir / "config.json";
    else if (name == "scene.mvr")
      outPath = tempDir / "scene.mvr";
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

  fs::remove_all(tempDir);
  if (ok) {
    ClearHistory();
    selectedFixtures.clear();
    selectedTrusses.clear();
    selectedSceneObjects.clear();
  }
  return ok;
}

void ConfigManager::Reset() {
  configData.clear();
  scene.Clear();
  ApplyDefaults();
  selectedFixtures.clear();
  selectedTrusses.clear();
  selectedSceneObjects.clear();
  currentLayer = DEFAULT_LAYER_NAME;
  ClearHistory();
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
  Snapshot snap{scene, selectedFixtures, selectedTrusses, selectedSceneObjects,
                description};
  undoStack.push_back(std::move(snap));
  if (undoStack.size() > maxHistory)
    undoStack.erase(undoStack.begin());
  redoStack.clear();
}

bool ConfigManager::CanUndo() const { return !undoStack.empty(); }

bool ConfigManager::CanRedo() const { return !redoStack.empty(); }

std::string ConfigManager::Undo() {
  if (undoStack.empty())
    return {};
  const Snapshot snap = undoStack.back();
  redoStack.push_back({scene, selectedFixtures, selectedTrusses,
                       selectedSceneObjects, snap.description});
  scene = snap.scene;
  selectedFixtures = snap.selFixtures;
  selectedTrusses = snap.selTrusses;
  selectedSceneObjects = snap.selSceneObjects;
  undoStack.pop_back();
  return snap.description;
}

std::string ConfigManager::Redo() {
  if (redoStack.empty())
    return {};
  const Snapshot snap = redoStack.back();
  undoStack.push_back({scene, selectedFixtures, selectedTrusses,
                       selectedSceneObjects, snap.description});
  scene = snap.scene;
  selectedFixtures = snap.selFixtures;
  selectedTrusses = snap.selTrusses;
  selectedSceneObjects = snap.selSceneObjects;
  redoStack.pop_back();
  return snap.description;
}

void ConfigManager::ClearHistory() {
  undoStack.clear();
  redoStack.clear();
}
