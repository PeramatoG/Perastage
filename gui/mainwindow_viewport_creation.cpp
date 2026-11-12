#include "mainwindow.h"
#include "guiconfigservices.h"
#include "summarypanel.h"
#include "startup_profile.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"
#include "patch/viewport_patch_workspace.h"

void MainWindow::Ensure3DViewport(bool commitAui) {
  if (startupSplashInitializationPending && startupMetrics_)
    ++startupMetrics_->ensure3DCalls;
  if (viewportPanel)
    return;
  if (startupSplashInitializationPending && startupMetrics_)
    ++startupMetrics_->viewer3DConstructions;
  int halfWidth = GetClientSize().GetWidth() / 2;
  viewport3DWorkspace = CreateDmxViewportWorkspace();
  viewportPanel = new Viewer3DPanel(viewport3DWorkspace);
  viewport3DWorkspace->SetViewport(viewportPanel);
  Viewer3DPanel::SetInstance(viewportPanel);
  if (startupSplashInitializationPending)
    viewportPanel->Bind(wxEVT_PAINT, &MainWindow::OnStartupScenePaint, this);
  viewportPanel->LoadCameraFromConfig();
  auiManager->AddPane(viewport3DWorkspace, wxAuiPaneInfo()
                                         .Name("3DViewport")
                                         .Caption(_("3D Viewport"))
                                         .Center()
                                         .Dockable(true)
                                         .CaptionVisible(true)
                                         .PaneBorder(false)
                                         .BestSize(halfWidth, 600)
                                         .MinSize(wxSize(200, 180))
                                         .CloseButton(true)
                                         .MaximizeButton(true));
  if (commitAui) {
    auiManager->Update();
    if (startupSplashInitializationPending && startupMetrics_)
      ++startupMetrics_->auiUpdates;
  }

  if (summaryPanel)
    summaryPanel->SetVisibleRefreshTargets(viewport2DPanel, viewportPanel);
  ApplyViewportMovementToolState();
}

void MainWindow::Ensure2DViewport(bool commitAui) {
  if (startupSplashInitializationPending && startupMetrics_)
    ++startupMetrics_->ensure2DCalls;
  if (viewport2DPanel)
    return;
  if (startupSplashInitializationPending && startupMetrics_)
    ++startupMetrics_->viewer2DConstructions;
  int halfWidth = GetClientSize().GetWidth() / 2;
  viewport2DWorkspace = CreateDmxViewportWorkspace();
  viewport2DPanel = new Viewer2DPanel(viewport2DWorkspace);
  viewport2DWorkspace->SetViewport(viewport2DPanel);
  std::weak_ptr<int> lifetimeToken = cursorStatusCallbackLifetimeToken;
  viewport2DPanel->SetCursorWorldPositionCallback(
      [this,
       lifetimeToken](const std::optional<std::array<float, 3>> &positionMeters,
          bool highlighted) {
        if (lifetimeToken.expired() || !GetStatusBar())
          return;
        if (highlighted)
          UpdateHighlightedWorldPositionInStatusBar(positionMeters);
        else
          UpdateCursorWorldPositionInStatusBar(positionMeters);
      });
  Viewer2DPanel::SetInstance(viewport2DPanel);
  viewport2DPanel->LoadViewFromConfig();
  auiManager->AddPane(viewport2DWorkspace, wxAuiPaneInfo()
                                           .Name("2DViewport")
                                           .Caption(_("2D Viewport"))
                                           .Center()
                                           .Dockable(true)
                                           .CaptionVisible(true)
                                           .PaneBorder(false)
                                           .BestSize(halfWidth, 600)
                                           .MinSize(wxSize(200, 180))
                                           .CloseButton(true)
                                           .MaximizeButton(true)
                                           .Hide());
  viewport2DPanel->UpdateScene();

  viewport2DRenderPanel = new Viewer2DRenderPanel(this);
  Viewer2DRenderPanel::SetInstance(viewport2DRenderPanel);
  auiManager->AddPane(viewport2DRenderPanel, wxAuiPaneInfo()
                                                 .Name("2DRenderOptions")
                                                 .Caption(_("2D Render Options"))
                                                 .Right()
                                                 .Layer(0)
                                                 .Row(0)
                                                 .Position(0)
                                                 .BestSize(220, 100)
                                                 .CloseButton(true)
                                                 .MaximizeButton(true)
                                                 .PaneBorder(true)
                                                 .Hide());

  if (summaryPanel)
    summaryPanel->SetVisibleRefreshTargets(viewport2DPanel, viewportPanel);
  ApplyViewportMovementToolState();

  if (commitAui) {
    auiManager->Update();
    if (startupSplashInitializationPending && startupMetrics_)
      ++startupMetrics_->auiUpdates;
  }

}
