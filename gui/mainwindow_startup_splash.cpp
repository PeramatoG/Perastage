#include "mainwindow.h"

#include "splashscreen.h"

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
  SplashScreen::SetMessage(_("Ready"));
  SplashScreen::Hide();

  if (deferredStartupOpenPath && !deferredStartupOpenPath->empty()) {
    const std::string startupPath = *deferredStartupOpenPath;
    deferredStartupOpenPath.reset();
    CallAfter([this, startupPath]() { OpenPathFromCommandLine(startupPath); });
  }
}
