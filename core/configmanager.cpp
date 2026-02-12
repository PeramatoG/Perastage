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
#include "json.hpp"
#include "LayoutManager.h"
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

bool LooksLikeZipFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;
  unsigned char signature[2] = {};
  file.read(reinterpret_cast<char *>(signature), sizeof(signature));
  return file.gcount() == static_cast<std::streamsize>(sizeof(signature)) &&
         signature[0] == 'P' && signature[1] == 'K';
}

bool LooksLikeJsonFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;
  char ch = '\0';
  while (file.get(ch)) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      return ch == '{' || ch == '[';
    }
  }
  return false;
}

std::string ToLowerCopy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}
} // namespace

ConfigManager::RevisionGuard::RevisionGuard(ConfigManager &cfg)
    : cfg(cfg), previous(cfg.suppressRevision) {
  cfg.suppressRevision = true;
}

ConfigManager::RevisionGuard::~RevisionGuard() {
  cfg.suppressRevision = previous;
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
  RegisterVariable("print_viewer2d_page_size", "float", 0.0f, 0.0f, 1.0f,
                   {"print_plan_page_size", "print_page_size"});
  RegisterVariable("print_viewer2d_landscape", "float", 0.0f, 0.0f, 1.0f,
                   {"print_plan_landscape", "print_landscape"});
  RegisterVariable("print_use_simplified_footprints", "float", 1.0f, 0.0f,
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
  RegisterVariable("view2d_offset_x", "float", 0.0f, -1000000.0f, 1000000.0f);
  RegisterVariable("view2d_offset_y", "float", 0.0f, -1000000.0f, 1000000.0f);
  RegisterVariable("view2d_zoom", "float", 1.0f, 0.1f, 100.0f);
  RegisterVariable("view2d_render_mode", "float", 2.0f, 0.0f, 3.0f);
  RegisterVariable("view2d_view", "float", 0.0f, 0.0f, 2.0f);
  RegisterVariable("view2d_dark_mode", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("viewer3d_aa_quality", "float", 1.0f, 0.0f, 2.0f);
  RegisterVariable("viewer3d_adaptive_line_profile", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("viewer3d_skip_labels_when_moving", "float", 1.0f, 0.0f,
                   1.0f);
  RegisterVariable("viewer3d_skip_outlines_when_moving", "float", 1.0f,
                   0.0f, 1.0f);
  RegisterVariable("viewer3d_skip_capture_when_moving", "float", 1.0f, 0.0f,
                   1.0f);
  RegisterVariable("viewer3d_fast_interaction_mode", "float", 1.0f, 0.0f,
                   1.0f);
  RegisterVariable("render_culling_enabled", "float", 1.0f, 0.0f, 1.0f);
  RegisterVariable("render_culling_min_pixels_3d", "float", 2.0f, 0.0f,
                   64.0f);
  RegisterVariable("render_culling_min_pixels_2d", "float", 1.0f, 0.0f,
                   64.0f);
  RegisterVariable("mvr_import_detailed_log", "float", 0.0f, 0.0f, 1.0f);
  RegisterVariable("label_optimizations_enabled", "float", 1.0f, 0.0f,
                   1.0f);
  RegisterVariable("label_max_fixtures", "float", 250.0f, 0.0f, 5000.0f);
  RegisterVariable("label_max_trusses", "float", 150.0f, 0.0f, 5000.0f);
  RegisterVariable("label_max_objects", "float", 150.0f, 0.0f, 5000.0f);
  LoadUserConfig();
  if (!HasKey("rider_autopatch"))
    SetValue("rider_autopatch", "1");
  if (!HasKey("rider_layer_mode"))
    SetValue("rider_layer_mode", "position");
  if (!HasKey("fixture_print_columns"))
    SetFixturePrintColumns({"position", "id", "type"});
  if (!HasKey("truss_print_columns"))
    SetTrussPrintColumns({"position", "type", "length"});
  if (!HasKey("support_print_columns"))
    SetSupportPrintColumns({"position", "type", "height"});
  if (!HasKey("sceneobject_print_columns"))
    SetSceneObjectPrintColumns({"position", "name", "type"});
  ApplyDefaults();
  layouts::LayoutManager::Get().LoadFromConfig(*this);
  layerVisibilityState.SetCurrentLayer(DEFAULT_LAYER_NAME);
}

ConfigManager &ConfigManager::Get() {
  static ConfigManager instance;
  return instance;
}

// -- Config key-value access --

void ConfigManager::SetValue(const std::string &key, const std::string &value) {
  auto prev = preferencesStore.GetValue(key);
  preferencesStore.SetValue(key, value);
  auto next = preferencesStore.GetValue(key);
  if (prev == next)
    return;
  if (!suppressRevision)
    projectSession.Touch();
}

std::optional<std::string>
ConfigManager::GetValue(const std::string &key) const {
  return preferencesStore.GetValue(key);
}

bool ConfigManager::HasKey(const std::string &key) const {
  return preferencesStore.HasKey(key);
}

void ConfigManager::RemoveKey(const std::string &key) {
  const bool had = preferencesStore.HasKey(key);
  preferencesStore.RemoveKey(key);
  if (had && !suppressRevision)
    projectSession.Touch();
}

void ConfigManager::ClearValues() {
  preferencesStore.ClearValues();
  if (!suppressRevision)
    projectSession.Touch();
}

void ConfigManager::RegisterVariable(const std::string &name,
                                     const std::string &type, float defVal,
                                     float minVal, float maxVal,
                                     std::vector<std::string> legacyNames) {
  preferencesStore.RegisterVariable(name, type, defVal, minVal, maxVal,
                                    std::move(legacyNames));
}

float ConfigManager::GetFloat(const std::string &name) const {
  return preferencesStore.GetFloat(name);
}

void ConfigManager::SetFloat(const std::string &name, float v) {
  auto prev = preferencesStore.GetValue(name);
  preferencesStore.SetFloat(name, v);
  if (!suppressRevision && prev != preferencesStore.GetValue(name))
    projectSession.Touch();
}

void ConfigManager::ApplyDefaults() { preferencesStore.ApplyDefaults(); }

std::vector<std::string> ConfigManager::GetFixturePrintColumns() const {
  return preferencesStore.GetFixturePrintColumns();
}

void ConfigManager::SetFixturePrintColumns(
    const std::vector<std::string> &cols) {
  preferencesStore.SetFixturePrintColumns(cols);
  if (!suppressRevision)
    projectSession.Touch();
}

std::vector<std::string> ConfigManager::GetTrussPrintColumns() const {
  return preferencesStore.GetTrussPrintColumns();
}

void ConfigManager::SetTrussPrintColumns(const std::vector<std::string> &cols) {
  preferencesStore.SetTrussPrintColumns(cols);
  if (!suppressRevision)
    projectSession.Touch();
}

std::vector<std::string> ConfigManager::GetSupportPrintColumns() const {
  return preferencesStore.GetSupportPrintColumns();
}

void ConfigManager::SetSupportPrintColumns(
    const std::vector<std::string> &cols) {
  preferencesStore.SetSupportPrintColumns(cols);
  if (!suppressRevision)
    projectSession.Touch();
}

std::vector<std::string> ConfigManager::GetSceneObjectPrintColumns() const {
  return preferencesStore.GetSceneObjectPrintColumns();
}

void ConfigManager::SetSceneObjectPrintColumns(
    const std::vector<std::string> &cols) {
  preferencesStore.SetSceneObjectPrintColumns(cols);
  if (!suppressRevision)
    projectSession.Touch();
}

std::unordered_set<std::string> ConfigManager::GetHiddenLayers() const {
  return layerVisibilityState.GetHiddenLayers();
}

void ConfigManager::SetHiddenLayers(
    const std::unordered_set<std::string> &layers) {
  layerVisibilityState.SetHiddenLayers(layers);
}

bool ConfigManager::IsLayerVisible(const std::string &layer) const {
  return layerVisibilityState.IsLayerVisible(layer);
}

void ConfigManager::SetLayerColor(const std::string &layer,
                                  const std::string &color) {
  layerVisibilityState.SetLayerColor(projectSession.GetScene(), layer, color);
}

std::optional<std::string>
ConfigManager::GetLayerColor(const std::string &layer) const {
  return layerVisibilityState.GetLayerColor(projectSession.GetScene(), layer);
}

std::vector<std::string> ConfigManager::GetLayerNames() const {
  return layerVisibilityState.GetLayerNames(projectSession.GetScene());
}

const std::string &ConfigManager::GetCurrentLayer() const {
  return layerVisibilityState.GetCurrentLayer();
}

void ConfigManager::SetCurrentLayer(const std::string &name) {
  layerVisibilityState.SetCurrentLayer(name);
}

// -- Scene access --

MvrScene &ConfigManager::GetScene() { return projectSession.GetScene(); }

const MvrScene &ConfigManager::GetScene() const {
  return projectSession.GetScene();
}

const std::vector<std::string> &ConfigManager::GetSelectedFixtures() const {
  return selectionState.GetSelectedFixtures();
}

void ConfigManager::SetSelectedFixtures(const std::vector<std::string> &uuids) {
  selectionState.SetSelectedFixtures(uuids);
}

const std::vector<std::string> &ConfigManager::GetSelectedTrusses() const {
  return selectionState.GetSelectedTrusses();
}

void ConfigManager::SetSelectedTrusses(const std::vector<std::string> &uuids) {
  selectionState.SetSelectedTrusses(uuids);
}

const std::vector<std::string> &ConfigManager::GetSelectedSupports() const {
  return selectionState.GetSelectedSupports();
}

void ConfigManager::SetSelectedSupports(const std::vector<std::string> &uuids) {
  selectionState.SetSelectedSupports(uuids);
}

const std::vector<std::string> &ConfigManager::GetSelectedSceneObjects() const {
  return selectionState.GetSelectedSceneObjects();
}

void ConfigManager::SetSelectedSceneObjects(
    const std::vector<std::string> &uuids) {
  selectionState.SetSelectedSceneObjects(uuids);
}

// -- Persistence --

bool ConfigManager::LoadFromFile(const std::string &path) {
  RevisionGuard guard(*this);
  bool ok = preferencesStore.LoadFromFile(path);
  if (!ok)
    return false;
  layouts::LayoutManager::Get().LoadFromConfig(*this);
  return true;
}

bool ConfigManager::SaveToFile(const std::string &path) const {
  return preferencesStore.SaveToFile(path);
}

bool ConfigManager::SaveProject(const std::string &path) {
  namespace fs = std::filesystem;
  TempDir tempDir("PerastageProj_");
  if (!tempDir.Valid())
    return false;

  layouts::LayoutManager::Get().SaveToConfig(*this);

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
  projectSession.MarkSaved();
  return true;
}

bool ConfigManager::LoadProject(const std::string &path) {
  namespace fs = std::filesystem;
  const bool hasUserView2dDarkMode = HasKey("view2d_dark_mode");
  const float userView2dDarkMode = GetFloat("view2d_dark_mode");
  auto restoreUserPreferences = [this, hasUserView2dDarkMode,
                                 userView2dDarkMode]() {
    if (!hasUserView2dDarkMode)
      return;
    RevisionGuard guard(*this);
    SetFloat("view2d_dark_mode", userView2dDarkMode);
  };

  if (!LooksLikeZipFile(path)) {
    if (LooksLikeJsonFile(path)) {
      bool ok = LoadFromFile(path);
      if (ok)
        restoreUserPreferences();
      return ok;
    }
    return false;
  }

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
  bool hasMvrSceneXml = false;

  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    std::string name = entry->GetName().ToStdString();
    std::string baseName =
        ToLowerCopy(fs::path(name).filename().string());
    fs::path outPath;
    if (baseName == "config.json")
      outPath = tempDir.Path() / "config.json";
    else if (baseName == "scene.mvr")
      outPath = tempDir.Path() / "scene.mvr";
    else {
      if (baseName == "generalscenedescription.xml")
        hasMvrSceneXml = true;
      continue;
    }

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

    if (baseName == "config.json")
      configPath = outPath;
    else if (baseName == "scene.mvr")
      scenePath = outPath;
  }

  if (configPath.empty() && scenePath.empty()) {
    if (hasMvrSceneXml) {
      std::string ext = ToLowerCopy(fs::path(path).extension().string());
      if (ext == ".mvr") {
        bool ok = MvrImporter::ImportAndRegister(path, false);
        if (ok)
          restoreUserPreferences();
        return ok;
      }

      fs::path tempMvrPath = tempDir.Path() / "legacy.mvr";
      std::error_code ec;
      fs::copy_file(path, tempMvrPath, fs::copy_options::overwrite_existing,
                    ec);
      if (ec)
        return false;
      bool ok = MvrImporter::ImportAndRegister(tempMvrPath.string(), false);
      if (ok)
        restoreUserPreferences();
      return ok;
    }
    if (LooksLikeJsonFile(path)) {
      bool ok = LoadFromFile(path);
      if (ok)
        restoreUserPreferences();
      return ok;
    }
    return false;
  }

  bool ok = true;
  if (!scenePath.empty())
    ok &= MvrImporter::ImportAndRegister(scenePath.string(), false);
  if (!configPath.empty())
    ok &= LoadFromFile(configPath.string());

  if (ok) {
    restoreUserPreferences();
    ClearHistory();
    selectionState.Clear();
    projectSession.ResetDirty();
  }
  return ok;
}

void ConfigManager::Reset() {
  RevisionGuard guard(*this);
  preferencesStore.ClearValues();
  projectSession.GetScene().Clear();
  if (!HasKey("rider_autopatch"))
    SetValue("rider_autopatch", "1");
  ApplyDefaults();
  layouts::LayoutManager::Get().ResetToDefault(*this);
  selectionState.Clear();
  layerVisibilityState.SetCurrentLayer(DEFAULT_LAYER_NAME);
  ClearHistory();
  projectSession.ResetDirty();
}

std::string ConfigManager::GetUserConfigFile() {
  return UserPreferencesStore::GetUserConfigFile();
}

bool ConfigManager::LoadUserConfig() {
  return preferencesStore.LoadUserConfig();
}

bool ConfigManager::SaveUserConfig() const {
  return preferencesStore.SaveUserConfig();
}

void ConfigManager::PushUndoState(const std::string &description) {
  historyManager.PushUndoState(projectSession.GetScene(), selectionState,
                               description);
  projectSession.Touch();
}

bool ConfigManager::CanUndo() const { return historyManager.CanUndo(); }

bool ConfigManager::CanRedo() const { return historyManager.CanRedo(); }

std::string ConfigManager::Undo() {
  return historyManager.Undo(projectSession.GetScene(), selectionState);
}

std::string ConfigManager::Redo() {
  projectSession.Touch();
  return historyManager.Redo(projectSession.GetScene(), selectionState);
}

void ConfigManager::ClearHistory() { historyManager.ClearHistory(); }

bool ConfigManager::IsDirty() const { return projectSession.IsDirty(); }

void ConfigManager::MarkSaved() { projectSession.MarkSaved(); }
