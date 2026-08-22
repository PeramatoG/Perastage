#include "mainwindow.h"

#include "splashscreen.h"

#include "diagnostics/DiagnosticLogger.h"
#include "services/fixture_symbol_preparation_service.h"
#include "startup_profile.h"
#include "consolepanel.h"

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
