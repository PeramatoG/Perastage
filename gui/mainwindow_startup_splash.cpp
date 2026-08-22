#include "mainwindow.h"

#include "splashscreen.h"

#include "diagnostics/DiagnosticLogger.h"
#include "services/fixture_symbol_preparation_service.h"

#include <chrono>

// Requests splash completion after the current startup operation finishes.
void MainWindow::RequestStartupSplashCompletion() {
  if (!startupSplashInitializationPending)
    return;
  startupSplashCloseRequested = true;
}

// Completes a deferred splash close from the idle event loop.
void MainWindow::OnStartupSplashCloseIdle(wxIdleEvent &event) {
  event.Skip();
  if (!startupSplashCloseRequested)
    return;

  startupSplashCloseRequested = false;
  CompleteStartupSplashInitialization();
}

// Hides the splash and opens any path deferred during startup.
void MainWindow::CompleteStartupSplashInitialization() {
  if (!startupSplashInitializationPending)
    return;

  startupSplashInitializationPending = false;
  const long long interactiveReadyMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - startupStartedAt_)
          .count();
  diagnostics::DiagnosticLogger::Info(
      "StartupProfile event=InteractiveReady duration_ms=" +
      std::to_string(interactiveReadyMs) +
      " apply_saved_layout=" + std::to_string(startupLayoutCommits_) +
      " activate_layout=" + std::to_string(startupLayoutActivations_) +
      " ensure_3d=" + std::to_string(startupEnsure3DCalls_) +
      " construct_3d=" + std::to_string(startup3DConstructions_) +
      " ensure_2d=" + std::to_string(startupEnsure2DCalls_) +
      " construct_2d=" + std::to_string(startup2DConstructions_) +
      " pstg_opens=" + std::to_string(currentProjectPath.empty() ? 0 : 1) +
      " archive_traversals=" +
      std::to_string(currentProjectPath.empty() ? 0 : 1));
  SplashScreen::SetMessage(_("Ready"));
  SplashScreen::Hide();
  if (fixtureSymbolPreparationService)
    fixtureSymbolPreparationService->ScheduleScan();
  diagnostics::DiagnosticLogger::Info(
      "StartupProfile event=PostStartupFixtureSymbolScanScheduled");

  if (deferredStartupOpenPath && !deferredStartupOpenPath->empty()) {
    const std::string startupPath = *deferredStartupOpenPath;
    deferredStartupOpenPath.reset();
    CallAfter([this, startupPath]() { OpenPathFromCommandLine(startupPath); });
  }
}
