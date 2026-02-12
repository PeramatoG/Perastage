#include "mainwindow_view_controller.h"

#include "mainwindow.h"
#include "viewer2dpanel.h"

void MainWindowViewController::OnToggleConsole(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("Console");
  pane.Show(!pane.IsShown());
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
}

void MainWindowViewController::OnToggleViewport2D(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  owner_.Ensure2DViewport();
  auto &pane = owner_.auiManager->GetPane("2DViewport");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
  if (pane.IsShown() && Viewer2DPanel::Instance())
    Viewer2DPanel::Instance()->Refresh();
}

void MainWindowViewController::OnToggleRender2D(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  owner_.Ensure2DViewport();
  auto &pane = owner_.auiManager->GetPane("2DRenderOptions");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleLayers(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("LayerPanel");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
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
  auto &pane = owner_.auiManager->GetPane("SummaryPanel");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  owner_.UpdateViewMenuChecks();
}

void MainWindowViewController::OnToggleRigging(wxCommandEvent &) {
  if (!owner_.auiManager)
    return;
  auto &pane = owner_.auiManager->GetPane("RiggingPanel");
  pane.Show(!pane.IsShown());
  owner_.auiManager->Update();
  if (pane.IsShown())
    owner_.RefreshRigging();
  owner_.UpdateViewMenuChecks();
}
