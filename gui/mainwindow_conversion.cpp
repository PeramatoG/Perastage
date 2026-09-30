/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mainwindow.h"

#include "command/command_scene_tools.h"
#include "configmanager.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "project_mutation_host.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <string>
#include <variant>
#include <vector>
#include <wx/intl.h>
#include <wx/msgdlg.h>

// Converts selected fixtures into hoist supports.
void MainWindow::OnConvertToHoist(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto selected = cfg.GetSelectedFixtures();
  if (selected.empty()) {
    wxMessageBox(_("Please select fixtures to convert first."),
                 _("Convert to Hoist"), wxOK | wxICON_INFORMATION);
    return;
  }

  auto &scene = cfg.GetScene();
  std::vector<std::string> convertible;
  for (const auto &uuid : selected) {
    if (scene.fixtures.contains(uuid) && !scene.supports.contains(uuid))
      convertible.push_back(uuid);
  }
  if (convertible.empty()) {
    wxMessageBox(_("The selected fixtures cannot be converted."),
                 _("Convert to Hoist"), wxOK | wxICON_INFORMATION);
    return;
  }

  scene_grouping::ObjectSelection semanticSelection{
      .fixtures = cfg.GetSelectedFixtures(),
      .trusses = cfg.GetSelectedTrusses(),
      .supports = cfg.GetSelectedSupports(),
      .sceneObjects = cfg.GetSelectedSceneObjects()};
  GuiProjectMutationHost mutationHost(cfg);
  perastage::command::ExecutionContext context{scene, semanticSelection,
                                               mutationHost};
  const auto result = perastage::command::scene_tools::ExecuteFixtureToSupport(
      {.fixtureUuids = convertible}, context);
  if (!result.Success()) {
    wxMessageBox(_("The selected fixtures cannot be converted."),
                 _("Convert to Hoist"), wxOK | wxICON_INFORMATION);
    return;
  }
  cfg.SetSelectedSupports(semanticSelection.supports);
  cfg.SetSelectedFixtures(semanticSelection.fixtures);

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
  RefreshRigging();

  wxMessageBox(wxString::Format(_("Converted %zu fixture(s) to hoists."),
                                semanticSelection.supports.size()),
               _("Convert to Hoist"), wxOK | wxICON_INFORMATION);
}

// Converts selected scene objects sharing the same model file into trusses.
void MainWindow::OnConvertSceneObjectsToTruss(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto selected = cfg.GetSelectedSceneObjects();
  if (selected.empty()) {
    wxMessageBox(_("Please select a scene object to convert first."),
                 _("Convert Scene Objects to Truss"),
                 wxOK | wxICON_INFORMATION);
    return;
  }

  auto &scene = cfg.GetScene();
  scene_grouping::ObjectSelection semanticSelection{
      .fixtures = cfg.GetSelectedFixtures(),
      .trusses = cfg.GetSelectedTrusses(),
      .supports = cfg.GetSelectedSupports(),
      .sceneObjects = cfg.GetSelectedSceneObjects()};
  GuiProjectMutationHost mutationHost(cfg);
  perastage::command::ExecutionContext context{scene, semanticSelection,
                                               mutationHost};
  const auto result =
      perastage::command::scene_tools::ExecuteSceneObjectsToTrusses(
          {.sourceSceneObjectUuid = selected.front()}, context);
  if (!result.Success()) {
    wxMessageBox(_("No scene objects with a valid model file were converted."),
                 _("Convert Scene Objects to Truss"),
                 wxOK | wxICON_INFORMATION);
    return;
  }

  cfg.SetSelectedSceneObjects(semanticSelection.sceneObjects);
  cfg.SetSelectedTrusses(semanticSelection.trusses);

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (trussPanel)
    trussPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  if (viewport2DPanel) {
    viewport2DPanel->UpdateScene();
    viewport2DPanel->Refresh();
  }
  RefreshSummary();
  RefreshRigging();

  std::string modelFile;
  for (const auto &output : result.outputs) {
    if (output.id == "model_file")
      modelFile = std::get<std::string>(output.value);
  }
  wxMessageBox(wxString::Format(
                   _("Converted %zu scene object(s) with model '%s' to truss."),
                   semanticSelection.trusses.size(),
                   wxString::FromUTF8(modelFile).c_str()),
               _("Convert Scene Objects to Truss"), wxOK | wxICON_INFORMATION);
}
