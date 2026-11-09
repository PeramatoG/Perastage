/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "mainwindow.h"

#include "guiconfigservices.h"
#include "configmanager.h"
#include "positiontoolpanel.h"

void MainWindow::CreatePositionTool() {
  positionToolPanel = new PositionToolPanel(
      this, guiConfigServices->LegacyConfigManager(),
      [this](const scene_grouping::ObjectSelection &selection, bool sceneChanged) {
        if (sceneChanged) RefreshAfterToolSceneUpdate(selection);
        else RefreshAfterToolSelectionUpdate(selection);
      });
  auiManager->AddPane(positionToolPanel, wxAuiPaneInfo()
      .Name("PositionTool").Caption(_("Position Tool"))
      .Right().Layer(1).Row(0).Position(2).BestSize(FromDIP(wxSize(295, 160)))
      .MinSize(FromDIP(wxSize(180, 85))).CloseButton(true));
}

void MainWindow::OnTogglePositionTool(wxCommandEvent &) {
  if (!auiManager) return;
  auto &pane = auiManager->GetPane("PositionTool");
  pane.Show(!pane.IsShown());
  auiManager->Update();
  UpdateViewMenuChecks();
}

// Migrate the original bottom/hidden default once, retaining later user layouts.
void MainWindow::InitializePositionToolSidebar(bool sceneWorkspace) {
  auto &preferences = guiConfigServices->Preferences();
  if (!sceneWorkspace || preferences.GetValue("ui_position_tool_sidebar_initialized"))
    return;
  auto &pane = auiManager->GetPane("PositionTool");
  pane.Dock().Right().Layer(1).Row(0).Position(2)
      .BestSize(FromDIP(wxSize(295, 160))).Show();
  auto &config = guiConfigServices->LegacyConfigManager();
  const auto dirty = config.CaptureDirtyState();
  preferences.SetValue("ui_position_tool_sidebar_initialized", "1");
  preferences.SetValue("layout_perspective", auiManager->SavePerspective().ToStdString());
  config.RestoreDirtyState(dirty);
}
