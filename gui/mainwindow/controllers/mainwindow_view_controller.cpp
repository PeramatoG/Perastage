#include "mainwindow_view_controller.h"

#include "mainwindow.h"

void MainWindowViewController::OnToggleConsole(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("Console");
  if (pane.IsShown()) {
    pane.Show(false);
  } else {
    pane.Show(true);
    if (owner_.bottomPanelsNotebook && owner_.consolePanel) {
      const int page =
          owner_.bottomPanelsNotebook->GetPageIndex(owner_.consolePanel);
      if (page != wxNOT_FOUND)
        owner_.bottomPanelsNotebook->SetSelection(static_cast<size_t>(page));
    }
  }
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleFixtures(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("DataNotebook");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleViewport(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  owner_.Ensure3DViewport();
  auto &pane = owner_.auiManager->GetPane("3DViewport");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
  owner_.SyncLayerVisibilityPanels();
}

void MainWindowViewController::OnToggleViewport2D(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  owner_.Ensure2DViewport();
  auto &pane = owner_.auiManager->GetPane("2DViewport");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
  owner_.SyncLayerVisibilityPanels();
}

void MainWindowViewController::OnToggleRender2D(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  owner_.Ensure2DViewport();
  auto &pane = owner_.auiManager->GetPane("LayerPanel");
  pane.Show(true);
  if (owner_.sidePanelsNotebook && owner_.viewport2DRenderPanel) {
    const int page = owner_.sidePanelsNotebook->GetPageIndex(
        owner_.viewport2DRenderPanel);
    if (page != wxNOT_FOUND)
      owner_.sidePanelsNotebook->SetSelection(static_cast<size_t>(page));
  }
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleLayers(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("LayerPanel");
  pane.Show(true);
  if (owner_.sidePanelsNotebook && owner_.layerPanel) {
    const int page = owner_.sidePanelsNotebook->GetPageIndex(owner_.layerPanel);
    if (page != wxNOT_FOUND)
      owner_.sidePanelsNotebook->SetSelection(static_cast<size_t>(page));
  }
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
  owner_.SyncLayerVisibilityPanels();
}

void MainWindowViewController::OnToggleLayouts(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("LayoutPanel");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleSummary(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("LayerPanel");
  pane.Show(true);
  if (owner_.sidePanelsNotebook && owner_.summaryPanel) {
    const int page =
        owner_.sidePanelsNotebook->GetPageIndex(owner_.summaryPanel);
    if (page != wxNOT_FOUND)
      owner_.sidePanelsNotebook->SetSelection(static_cast<size_t>(page));
  }
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleRigging(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("Console");
  pane.Show(true);
  if (owner_.bottomPanelsNotebook && owner_.riggingPanel) {
    const int page =
        owner_.bottomPanelsNotebook->GetPageIndex(owner_.riggingPanel);
    if (page != wxNOT_FOUND)
      owner_.bottomPanelsNotebook->SetSelection(static_cast<size_t>(page));
  }
  owner_.auiManager->Update();
  if (pane.IsShown())
    owner_.RefreshRigging();
  owner_.UpdateViewMenuChecks();
}
