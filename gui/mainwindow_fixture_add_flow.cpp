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
#include "mainwindow.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/msgdlg.h>
#include <wx/string.h>

#include "addfixturedialog.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "viewer3dpanel.h"

namespace {

std::string Utf8StemFromPath(const std::string &path) {
  const auto stem = std::filesystem::path(path).stem().u8string();
  return std::string(stem.begin(), stem.end());
}

} // namespace

void MainWindow::AddFixtureFromGdtfPath(const std::string &gdtfPath,
                                        const std::string &suggestedName) {
  if (gdtfPath.empty()) {
    wxMessageBox("No GDTF file selected.", "Add fixture", wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Add fixture failed: empty GDTF path");
    return;
  }
  const wxString gdtfPathWx = wxString::FromUTF8(gdtfPath);

  if (!std::filesystem::exists(std::filesystem::u8path(gdtfPath))) {
    wxMessageBox("The selected GDTF file does not exist.", "Add fixture",
                 wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage(
          wxString::Format("[ERROR] Add fixture failed: file not found %s",
                           gdtfPathWx));
    return;
  }

  std::string defaultName = suggestedName;
  if (defaultName.empty())
    defaultName = GetGdtfFixtureName(gdtfPath);
  if (defaultName.empty())
    defaultName = Utf8StemFromPath(gdtfPath);

  if (consolePanel)
    consolePanel->AppendMessage(
        wxString::Format("[INFO] Loading GDTF fixture from: %s", gdtfPathWx));

  std::vector<std::string> modes = GetGdtfModes(gdtfPath);
  if (modes.empty()) {
    wxMessageBox("Could not read fixture modes from the selected GDTF file.",
                 "Add fixture", wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage(
          wxString::Format("[ERROR] Add fixture failed: no modes found in %s",
                           gdtfPathWx));
    return;
  }

  AddFixtureDialog dlg(this, wxString::FromUTF8(defaultName), modes);
  if (dlg.ShowModal() != wxID_OK)
    return;

  float weight = 0.0f;
  float power = 0.0f;
  GetGdtfProperties(gdtfPath, weight, power);
  std::string defaultColor = GetGdtfModelColor(gdtfPath);
  if (auto dictEntry = GdtfDictionary::Get(defaultName)) {
    if (!dictEntry->color.empty())
      defaultColor = dictEntry->color;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  int count = dlg.GetUnitCount();
  std::string name = dlg.GetFixtureName();
  int startId = dlg.GetFixtureId();
  std::string mode = dlg.GetMode();

  namespace fs = std::filesystem;
  cfg.PushUndoState("add fixture");
  auto &sceneRef = cfg.GetScene();
  std::string base = sceneRef.basePath;
  std::string spec = gdtfPath;
  if (!base.empty()) {
    fs::path abs = fs::absolute(gdtfPath);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      spec = fs::relative(abs, b).string();
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : sceneRef.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    sceneRef.layers[layer.uuid] = layer;
  }

  int maxId = 0;
  for (const auto &[uuid, fix] : sceneRef.fixtures) {
    if (fix.fixtureId > maxId)
      maxId = fix.fixtureId;
  }
  if (startId <= 0)
    startId = maxId + 1;

  for (int i = 0; i < count; ++i) {
    Fixture f;
    f.uuid = wxString::Format("uuid_%lld_%d", static_cast<long long>(baseId), i)
                 .ToStdString();
    f.instanceName = name;
    f.typeName = defaultName;
    f.fixtureId = startId + i;
    f.gdtfSpec = spec;
    f.gdtfMode = mode;
    f.layer = layerName;
    f.weightKg = weight;
    f.powerConsumptionW = power;
    f.color = defaultColor;
    sceneRef.fixtures[f.uuid] = f;
  }

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}
