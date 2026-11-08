/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "mainwindow.h"

#include "guiconfigservices.h"
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
      .Bottom().Layer(1).Row(1).BestSize(FromDIP(wxSize(420, 90)))
      .MinSize(FromDIP(wxSize(180, 85))).CloseButton(true).Hide());
}

void MainWindow::OnTogglePositionTool(wxCommandEvent &) {
  if (!auiManager) return;
  auto &pane = auiManager->GetPane("PositionTool");
  pane.Show(!pane.IsShown());
  auiManager->Update();
  UpdateViewMenuChecks();
}
