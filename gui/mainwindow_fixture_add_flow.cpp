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
#include "filesystem_path_utils.h"
#include "mainwindow.h"
#include "uuidutils.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/msgdlg.h>
#include <wx/string.h>

#include "addfixturedialog.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "continuous_placement_type.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtfdictionary.h"
#include "gdtf_fixture_insertion_preparation.h"
#include "guiconfigservices.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "diagnostics/DiagnosticLogger.h"

namespace {

// Returns a UTF-8 filename stem for fallback fixture naming.
std::string Utf8StemFromPath(const std::string &path) {
  const auto stem = std::filesystem::path(path).stem().u8string();
  return std::string(stem.begin(), stem.end());
}

// Writes fixture insertion preparation diagnostics to the app log and console.
void LogPreparationDiagnostics(
    const gdtf::GdtfFixtureInsertionPreparation &preparation,
    ConsolePanel *consolePanel) {
  const std::string filename =
      diagnostics::DiagnosticLogger::FileNameOnly(
          preparation.sourcePath.generic_string());
  for (const auto &diagnostic : preparation.diagnostics) {
    const std::string message =
        "GDTF insertion " + diagnostic.stage + " [" + diagnostic.code +
        "] " + filename + ": " + diagnostic.message +
        (diagnostic.detail.empty() ? std::string()
                                   : " (" + diagnostic.detail + ")");
    if (diagnostic.severity ==
        gdtf::FixtureInsertionDiagnosticSeverity::Error) {
      diagnostics::DiagnosticLogger::Error(message);
      if (consolePanel)
        consolePanel->AppendMessage("[ERROR] " + message);
    } else if (diagnostic.severity ==
               gdtf::FixtureInsertionDiagnosticSeverity::Warning) {
      diagnostics::DiagnosticLogger::Warning(message);
      if (consolePanel)
        consolePanel->AppendMessage("[WARN] " + message);
    } else {
      diagnostics::DiagnosticLogger::Info(message);
    }
  }
}

} // namespace

// Adds fixtures from a validated GDTF path using fixed or continuous placement.
void MainWindow::AddFixtureFromGdtfPath(const std::string &gdtfPath,
                                        const std::string &suggestedName) {
  if (gdtfPath.empty()) {
    wxMessageBox(_("No GDTF file selected."), _("Add fixture"), wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage(
          "[ERROR] Add fixture failed: empty GDTF path");
    return;
  }
  const wxString gdtfPathWx = wxString::FromUTF8(gdtfPath);

  const auto preparation =
      gdtf::PrepareGdtfFixtureInsertion(PathUtils::PathFromUtf8(gdtfPath));
  LogPreparationDiagnostics(preparation, consolePanel);
  if (!preparation.success) {
    const auto *error = gdtf::FirstError(preparation);
    const std::string reason =
        error ? error->message : "The selected GDTF file could not be read.";
    wxMessageBox(wxString(_("Could not add the selected GDTF fixture.\n")) +
                     wxString::FromUTF8(reason),
                 _("Add fixture"), wxOK | wxICON_ERROR);
    return;
  }

  std::string defaultName =
      suggestedName.empty() ? preparation.fixtureDisplayName : suggestedName;
  if (defaultName.empty())
    defaultName = Utf8StemFromPath(gdtfPath);

  if (consolePanel)
    consolePanel->AppendMessage(
        wxString::Format("[INFO] Loading GDTF fixture from: %s", gdtfPathWx));

  std::vector<std::string> modes = preparation.dmxModeNames;

  diagnostics::DiagnosticLogger::Info(
      "GDTF insertion dialog opened for " +
      diagnostics::DiagnosticLogger::FileNameOnly(gdtfPath) + " modes=" +
      std::to_string(modes.size()));
  AddFixtureDialog dlg(this, wxString::FromUTF8(defaultName), modes);
  if (dlg.ShowModal() != wxID_OK)
    return;

  float weight = preparation.weightKg.value_or(0.0f);
  float power = preparation.powerConsumptionW.value_or(0.0f);
  std::string defaultColor = preparation.modelColorHex;
  if (auto dictEntry = GdtfDictionary::Get(defaultName)) {
    if (!dictEntry->visualColorHex.empty())
      defaultColor = dictEntry->visualColorHex;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const bool continuousPlacement = dlg.IsContinuousPlacementEnabled();
  int count = continuousPlacement ? 1 : dlg.GetUnitCount();
  std::string name = dlg.GetFixtureName();
  int startId = dlg.GetFixtureId();
  std::string mode = dlg.GetMode();

  namespace fs = std::filesystem;
  cfg.PushUndoState("add fixture");
  auto &sceneRef = cfg.GetScene();
  std::string base = sceneRef.basePath;
  std::string spec = gdtfPath;
  if (!base.empty()) {
    fs::path abs = fs::absolute(PathUtils::PathFromUtf8(gdtfPath));
    fs::path b = fs::absolute(PathUtils::PathFromUtf8(base));
    const std::string absUtf8 = PathUtils::PathToUtf8(abs);
    const std::string baseUtf8 = PathUtils::PathToUtf8(b);
    if (absUtf8.rfind(baseUtf8, 0) == 0)
      spec = PathUtils::PathToUtf8(fs::relative(abs, b));
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

  std::string firstAddedFixtureUuid;
  for (int i = 0; i < count; ++i) {
    Fixture f;
    f.uuid = GenerateUuid();
    if (firstAddedFixtureUuid.empty())
      firstAddedFixtureUuid = f.uuid;
    f.instanceName = name;
    f.typeName = defaultName;
    f.fixtureId = startId + i;
    f.gdtfSpec = spec;
    f.gdtfMode = mode;
    f.layer = layerName;
    f.weightKg = weight;
    f.powerConsumptionW = power;
    f.physicalPropertiesSource = FixturePhysicalPropertiesSource::Gdtf;
    f.physicalPropertiesDirty = false;
    f.visualColorHex = defaultColor;
    sceneRef.fixtures[f.uuid] = f;
  }

  const std::string continuousFixtureUuid =
      continuousPlacement ? firstAddedFixtureUuid : std::string();

  RefreshAfterSceneChange(true);
  if (continuousPlacement) {
    if (viewport2DPanel && viewport2DPanel->IsShownOnScreen())
      viewport2DPanel->BeginContinuousPlacement(
          ContinuousPlacementType::Fixture, continuousFixtureUuid);
    else if (viewportPanel && viewportPanel->IsShownOnScreen())
      viewportPanel->BeginContinuousPlacement(
          ContinuousPlacementType::Fixture, continuousFixtureUuid);
  }
  RefreshSummary();
  diagnostics::DiagnosticLogger::Info(
      "GDTF fixture insertion completed for " +
      diagnostics::DiagnosticLogger::FileNameOnly(gdtfPath) + " count=" +
      std::to_string(count) + " mode=" + mode);
}
