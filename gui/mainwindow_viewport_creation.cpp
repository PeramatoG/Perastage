#include "mainwindow.h"
#include "guiconfigservices.h"
#include "summarypanel.h"
#include "startup_profile.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

void MainWindow::Ensure3DViewport(bool commitAui) {
  if (startupSplashInitializationPending && startupMetrics_)
    ++startupMetrics_->ensure3DCalls;
  if (viewportPanel)
    return;
  if (startupSplashInitializationPending && startupMetrics_)
    ++startupMetrics_->viewer3DConstructions;
  int halfWidth = GetClientSize().GetWidth() / 2;
  viewportPanel = new Viewer3DPanel(this);
  Viewer3DPanel::SetInstance(viewportPanel);
  viewportPanel->LoadCameraFromConfig();
  auiManager->AddPane(viewportPanel, wxAuiPaneInfo()
                                         .Name("3DViewport")
                                         .Caption(_("3D Viewport"))
                                         .Center()
                                         .Dockable(true)
                                         .CaptionVisible(true)
                                         .PaneBorder(false)
                                         .BestSize(halfWidth, 600)
                                         .MinSize(wxSize(200, 600))
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
  viewport2DPanel = new Viewer2DPanel(this);
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
  auiManager->AddPane(viewport2DPanel, wxAuiPaneInfo()
                                           .Name("2DViewport")
                                           .Caption(_("2D Viewport"))
                                           .Center()
                                           .Dockable(true)
                                           .CaptionVisible(true)
                                           .PaneBorder(false)
                                           .BestSize(halfWidth, 600)
                                           .MinSize(wxSize(200, 600))
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
