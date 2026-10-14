#include "mainwindow.h"
#include "native_gui_publication.h"
#include <wx/wupdlock.h>

#include "splashscreen.h"

#include "diagnostics/DiagnosticLogger.h"
#include "services/fixture_symbol_preparation_service.h"
#include "startup_profile.h"
#include "consolepanel.h"
#include "viewer3dpanel.h"

#include <chrono>

// Requests splash completion after the current startup operation finishes.
void MainWindow::RequestStartupSplashCompletion() {
  if (!startupSplashInitializationPending)
    return;
  startupSplashCloseRequested = true;
  wxWakeUpIdle();
}

// Completes a deferred splash close from the idle event loop.
void MainWindow::OnStartupSplashCloseIdle(wxIdleEvent &event) {
  event.Skip();
  if (!startupSplashCloseRequested || startupProjectLoadPending ||
      initialMainWindowPublished)
    return;

  PublishInitialMainWindow();
  // A second idle event can be starved by progressive viewport work. Queue the
  // native shell publication independently of the scene presentation gate.
  CallAfter(&MainWindow::CompleteStartupSplashInitialization);
}

// Publishes the fully composed initial frame exactly once behind the splash.
void MainWindow::PublishInitialMainWindow() {
  if (initialMainWindowPublished)
    return;
  gui::InitializeNativeGuiBackgrounds(*this);
  wxWindowUpdateLocker composition(this);
  Maximize(true);
  if (auiManager) {
    auiManager->Update();
    if (startupSplashInitializationPending && startupMetrics_)
      ++startupMetrics_->auiUpdates;
  }
  Layout();
  Show(true);
  SendSizeEvent();
  initialMainWindowPublished = true;
}

// Publishes the native shell once, then waits for the selected view boundary.
void MainWindow::CompleteStartupSplashInitialization() {
  if (!startupSplashInitializationPending || !initialMainWindowPublished ||
      startupNativeGuiPublished)
    return;

  if (auiManager) {
    auiManager->Update();
    if (startupMetrics_)
      ++startupMetrics_->auiUpdates;
  }
  Layout();
  SendSizeEvent();
  gui::PublishNativeGuiChildren(*this);
  startupNativeGuiPublished = true;
  TryFinishStartupPresentation();
}

// Observe a natural viewer paint; never request a frame or synchronize resources.
void MainWindow::OnStartupScenePaint(wxPaintEvent &event) {
  event.Skip();
  if (!startupSplashInitializationPending || startupScenePaintCheckQueued)
    return;
  startupScenePaintCheckQueued = true;
  CallAfter([this]() {
    startupScenePaintCheckQueued = false;
    if (!startupSplashInitializationPending || !viewportPanel)
      return;
    // The renderer's paint handler has returned, so this snapshot follows its
    // normal presentation rather than merely a resource-pump progress update.
    startupSceneSourcesPresented = viewportPanel->IsShownOnScreen() &&
        viewportPanel->IsEnabled() &&
        !viewportPanel->HasPendingSourceResourceLoading();
    TryFinishStartupPresentation();
  });
}

void MainWindow::TryFinishStartupPresentation() {
  const bool visible3D = viewportPanel && viewportPanel->IsShownOnScreen();
  if (!gui::CanDismissStartupSplash(
          startupNativeGuiPublished, visible3D, startupSceneSourcesPresented,
          visible3D && viewportPanel->HasPendingSourceResourceLoading(),
          IsEnabled() && (!visible3D || viewportPanel->IsEnabled()),
          viewportInteractionLockDepth > 0))
    return;
  FinishStartupSplashInitialization();
}

// Dismiss protection only after the selected view's presentation is usable.
void MainWindow::FinishStartupSplashInitialization() {
  if (!startupSplashInitializationPending)
    return;
  startupSplashCloseRequested = false;
  startupSplashInitializationPending = false;
  const long long interactiveReadyMs = startupMetrics_
      ? std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startupMetrics_->startedAt)
            .count()
      : 0;
  if (startupMetrics_)
    startupMetrics_->interactiveReady = true;
  const startup::Metrics emptyMetrics;
  const startup::Metrics &metrics =
      startupMetrics_ ? *startupMetrics_ : emptyMetrics;
  const std::string summary =
      startup::FormatInteractiveReadySummary(metrics, interactiveReadyMs);
  diagnostics::DiagnosticLogger::Info(summary);
  if (consolePanel)
    consolePanel->AppendMessage("[INFO] " + wxString::FromUTF8(summary));
  SplashScreen::SetMessage(_("Ready"));
  if (viewportPanel)
    viewportPanel->Unbind(wxEVT_PAINT, &MainWindow::OnStartupScenePaint, this);
  SplashScreen::Hide();
  if (viewportPanel && viewportPanel->IsShownOnScreen())
    viewportPanel->SetFocus();
  const bool waitForActiveLayout =
      startupMetrics_ && startupMetrics_->finalViewMode == "layout_mode_view";
  startupFixtureScanDeferredForLayout_ = waitForActiveLayout;
  if (!waitForActiveLayout && fixtureSymbolPreparationService) {
    fixtureSymbolPreparationService->ScheduleScan();
    diagnostics::DiagnosticLogger::Info(
        "StartupProfile event=PostStartupFixtureSymbolScanScheduled");
  }

  if (deferredStartupOpenPath && !deferredStartupOpenPath->empty()) {
    const std::string startupPath = *deferredStartupOpenPath;
    deferredStartupOpenPath.reset();
    CallAfter([this, startupPath]() { OpenPathFromCommandLine(startupPath); });
  }
}
