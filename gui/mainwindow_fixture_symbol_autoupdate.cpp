#include "mainwindow.h"

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include "configmanager.h"
#include "consolepanel.h"
#include "fixture.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "filesystem_path_utils.h"
#include "guiconfigservices.h"
#include "opaque_pass_utils.h"
#include "project_symbol_cache_snapshot.h"
#include "splashscreen.h"
#include "symbol_cache_manifest.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "windows/symbol_fixture_applier.h"

#include <wx/timer.h>
#include <wx/log.h>

namespace {
const int kStatusClearTimerId = wxWindow::NewControlId();
std::unordered_map<MainWindow *, std::unique_ptr<wxTimer>> g_statusClearTimers;
std::unordered_map<MainWindow *, std::string> g_pendingStatusText;
std::unordered_map<MainWindow *, wxEvtHandler *> g_statusTimerHandlers;

// Returns whether fixture-symbol timing diagnostics are enabled for this build.
constexpr bool FixtureSymbolDiagnosticsEnabled() {
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}

class ScopedFixtureSymbolDiagnostic {
public:
  // Creates a per-fixture diagnostic using the GUI build enablement policy.
  ScopedFixtureSymbolDiagnostic(std::string label, std::string key)
      : label_(std::move(label)), key_(std::move(key)),
        timings_(FixtureSymbolDiagnosticsEnabled()) {}

  // Emits a compact debug record only when diagnostics are enabled.
  ~ScopedFixtureSymbolDiagnostic() {
    if (!timings_.Enabled())
      return;
    const std::string message = timings_.Format(label_, key_, outcome_);
    wxLogDebug("%s", message.c_str());
  }

  // Returns the optional timing sink passed through production boundaries.
  symbols::FixtureSymbolTimings &Timings() { return timings_; }

  // Records the final work outcome for debug formatting.
  void SetOutcome(symbols::FixtureSymbolOutcome outcome) { outcome_ = outcome; }

  // Replaces the fallback UUID with the resolved generation identity.
  void SetKey(std::string key) { key_ = std::move(key); }

private:
  std::string label_;
  std::string key_;
  symbols::FixtureSymbolTimings timings_;
  symbols::FixtureSymbolOutcome outcome_ = symbols::FixtureSymbolOutcome::Failed;
};

// Builds a human-readable fixture label for progress and error reporting.
std::string BuildFixtureLabel(const Fixture &fixture) {
  if (!fixture.typeName.empty())
    return fixture.typeName;
  if (!fixture.instanceName.empty())
    return fixture.instanceName;
  if (!fixture.gdtfSpec.empty())
    return std::filesystem::path(fixture.gdtfSpec).filename().string();
  return "unknown fixture";
}


// Builds a manifest validation request for the current fixture and resolved GDTF path.
bool BuildFixtureSymbolCacheRequest(
    const Fixture &fixture,
    const gui::fixtures::FixtureGdtfResolution &resolution,
    symbol_cache::ValidationRequest &request, std::string &errorMessage) {
  const std::string selectedPath = resolution.selectedPath.empty()
                                       ? resolution.scenePath
                                       : resolution.selectedPath;
  if (selectedPath.empty()) {
    errorMessage = "fixture GDTF path is empty";
    return false;
  }

  std::string hashError;
  const std::string contentHash =
      symbol_cache::ComputeGdtfSemanticFingerprint(selectedPath, hashError);
  if (contentHash.empty()) {
    errorMessage = hashError.empty() ? "could not hash fixture GDTF" : hashError;
    return false;
  }

  if (!symbol_cache::BuildFixtureSymbolGenerationIdentity(
          fixture.gdtfSpec, fixture.gdtfMode,
          symbol_cache::kCurrentPerastageSymbolFormatVersion, contentHash,
          fixture.typeName, request.generationIdentity, errorMessage))
    return false;
  request.fixtureTypeName = fixture.typeName;
  request.gdtfSpec = fixture.gdtfSpec;
  request.gdtfContentHash = contentHash;
  request.requiredViews = symbol_cache::RequiredPerastageSymbolViews();
  return true;
}

// Resolves a fixture GDTF and builds the corresponding manifest validation request.
bool ResolveFixtureSymbolCacheRequest(const Fixture &fixture, const MvrScene &scene,
                                      symbol_cache::ValidationRequest &request,
                                      gui::fixtures::FixtureGdtfResolution &resolution,
                                      std::string &errorMessage) {
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(
          fixture, scene, resolution, errorMessage, "symbol-cache")) {
    return false;
  }
  return BuildFixtureSymbolCacheRequest(fixture, resolution, request,
                                        errorMessage);
}

// Updates status/console text and maintains a per-window timer that clears transient status messages.
void ReportFixtureAutoUpdate(MainWindow &window, ConsolePanel *console,
                             const std::string &message,
                             bool logToConsole = true) {
  MainWindow *windowPtr = &window;
  auto timerIt = g_statusClearTimers.find(windowPtr);
  if (timerIt == g_statusClearTimers.end()) {
    auto timer = std::make_unique<wxTimer>(&window, kStatusClearTimerId);
    auto *handler = new wxEvtHandler();
    handler->Bind(
        wxEVT_TIMER,
        [windowPtr](wxTimerEvent &) {
          auto pendingIt = g_pendingStatusText.find(windowPtr);
          if (pendingIt == g_pendingStatusText.end())
            return;
          if (windowPtr->GetStatusBar() &&
              windowPtr->GetStatusBar()->GetStatusText(0) ==
                  wxString::FromUTF8(pendingIt->second))
            windowPtr->SetStatusText("", 0);
          g_pendingStatusText.erase(pendingIt);
        },
        kStatusClearTimerId);
    window.PushEventHandler(handler);
    g_statusTimerHandlers[windowPtr] = handler;
    timerIt = g_statusClearTimers.emplace(windowPtr, std::move(timer)).first;
  }

  window.SetStatusText(wxString::FromUTF8(message), 0);
  g_pendingStatusText[windowPtr] = message;
  timerIt->second->StartOnce(10000);

  if (console && logToConsole)
    console->AppendMessage(wxString::FromUTF8(message));
}

} // namespace

// Cleans up the per-window fixture auto-update timer and event handler bindings.
void MainWindow::CleanupFixtureAutoUpdateStatusTimer() {
  MainWindow *windowPtr = this;
  if (auto timerIt = g_statusClearTimers.find(windowPtr);
      timerIt != g_statusClearTimers.end()) {
    timerIt->second->Stop();
    g_statusClearTimers.erase(timerIt);
  }

  if (auto pendingIt = g_pendingStatusText.find(windowPtr);
      pendingIt != g_pendingStatusText.end()) {
    g_pendingStatusText.erase(pendingIt);
  }

  if (auto handlerIt = g_statusTimerHandlers.find(windowPtr);
      handlerIt != g_statusTimerHandlers.end()) {
    wxEvtHandler *handler = handlerIt->second;
    if (handler != nullptr)
      RemoveEventHandler(handler);
    g_statusTimerHandlers.erase(handlerIt);
  }
}

// Summarizes fixture auto-update results for completion reporting.
std::string MainWindow::BuildFixtureSymbolAutoUpdateSummary() const {
  std::ostringstream summary;
  summary << "Fixture symbol auto-update summary:";

  if (fixtureSymbolAutoUpdateGeneratedTypes.empty()) {
    summary << " no fixture types generated.";
  } else {
    summary << " generated " << fixtureSymbolAutoUpdateGeneratedTypes.size()
            << " fixture type(s): ";
    for (size_t i = 0; i < fixtureSymbolAutoUpdateGeneratedTypes.size(); ++i) {
      if (i > 0)
        summary << ", ";
      summary << fixtureSymbolAutoUpdateGeneratedTypes[i];
    }
    summary << ".";
  }

  if (fixtureSymbolAutoUpdateErrors.empty()) {
    summary << " Errors: none.";
  } else {
    summary << " Errors (" << fixtureSymbolAutoUpdateErrors.size() << "): ";
    const size_t maxErrors = 3;
    for (size_t i = 0; i < fixtureSymbolAutoUpdateErrors.size() && i < maxErrors;
         ++i) {
      if (i > 0)
        summary << " | ";
      summary << fixtureSymbolAutoUpdateErrors[i];
    }
    if (fixtureSymbolAutoUpdateErrors.size() > maxErrors)
      summary << " | ...";
    summary << ".";
  }

  return summary.str();
}

// Starts fixture symbol auto-update for the currently loaded scene.
void MainWindow::RequestFixtureSymbolAutoUpdate() {
  StartFixtureSymbolAutoUpdateForLoadedScene();
}

void MainWindow::StartFixtureSymbolAutoUpdateForLoadedScene() {
  fixtureSymbolAutoUpdateQueue.clear();
  fixtureSymbolAutoUpdateProcessedKeys.clear();
  fixtureSymbolPendingLibrarySyncUuids.clear();
  fixtureSymbolAutoUpdateGeneratedTypes.clear();
  fixtureSymbolAutoUpdateErrors.clear();
  fixtureSymbolAutoUpdateGeneratedTypeSet.clear();
  fixtureSymbolAutoUpdateRunning = false;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  for (const auto &[uuid, fixture] : cfg.GetScene().fixtures) {
    fixtureSymbolAutoUpdateQueue.push_back(uuid);
  }

  fixtureSymbolAutoUpdateProcessedKeys.clear();

  if (fixtureSymbolAutoUpdateQueue.empty()) {
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: no fixtures queued.",
                            false);
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update summary: no fixtures queued.");
    if (fixtureSymbolAutoUpdateCompletionCallback) {
      auto completionCallback = fixtureSymbolAutoUpdateCompletionCallback;
      fixtureSymbolAutoUpdateCompletionCallback = nullptr;
      completionCallback();
    }
    return;
  }

  fixtureSymbolAutoUpdateRunning = true;
  ReportFixtureAutoUpdate(*this, consolePanel,
                          "Fixture symbol auto-update: started.", false);
  CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
}

// Processes queued fixture entries and auto-generates missing fixture symbols.
void MainWindow::ProcessNextFixtureSymbolAutoUpdate() {
  if (!fixtureSymbolAutoUpdateRunning)
    return;

  if (fixtureSymbolAutoUpdateQueue.empty()) {
    fixtureSymbolAutoUpdateRunning = false;
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: completed.", false);
    ReportFixtureAutoUpdate(*this, consolePanel,
                            BuildFixtureSymbolAutoUpdateSummary());
    if (fixtureSymbolAutoUpdateCompletionCallback) {
      auto completionCallback = fixtureSymbolAutoUpdateCompletionCallback;
      fixtureSymbolAutoUpdateCompletionCallback = nullptr;
      completionCallback();
    }
    return;
  }

  const std::string fixtureUuid = fixtureSymbolAutoUpdateQueue.back();
  fixtureSymbolAutoUpdateQueue.pop_back();

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto fixtureIt = cfg.GetScene().fixtures.find(fixtureUuid);
  if (fixtureIt == cfg.GetScene().fixtures.end()) {
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  const Fixture &fixture = fixtureIt->second;
  const std::string fixtureLabel = BuildFixtureLabel(fixture);
  ScopedFixtureSymbolDiagnostic diagnostic(fixtureLabel, fixtureUuid);

  symbol_cache::ValidationRequest cacheRequest;
  gui::fixtures::FixtureGdtfResolution cacheResolution;
  std::string cacheError;
  bool resolved = false;
  {
    symbols::ScopedFixtureSymbolPhase phase(&diagnostic.Timings(),
                                            symbols::FixtureSymbolPhase::Resolve);
    resolved = gui::fixtures::ResolveFixtureGdtfDeterministic(
        fixture, cfg.GetScene(), cacheResolution, cacheError, "symbol-cache");
  }
  bool hasCacheRequest = false;
  if (resolved) {
    symbols::ScopedFixtureSymbolPhase phase(
        &diagnostic.Timings(), symbols::FixtureSymbolPhase::Fingerprint);
    hasCacheRequest = BuildFixtureSymbolCacheRequest(
        fixture, cacheResolution, cacheRequest, cacheError);
  }
  if (hasCacheRequest) {
    diagnostic.SetKey(cacheRequest.generationIdentity.key);
    if (!fixtureSymbolAutoUpdateProcessedKeys
             .insert(cacheRequest.generationIdentity.key)
             .second) {
      CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
      return;
    }
    symbol_cache::ValidationResult cacheResult;
    {
      symbols::ScopedFixtureSymbolPhase phase(
          &diagnostic.Timings(), symbols::FixtureSymbolPhase::Validation);
      cacheResult = cfg.GetSymbolCacheManifest().ValidateFixture(cacheRequest);
    }
    if (cacheResult.valid) {
      diagnostic.SetOutcome(symbols::FixtureSymbolOutcome::Skipped);
      ReportFixtureAutoUpdate(
          *this, consolePanel,
          "Fixture symbol auto-update: skipped '" + fixtureLabel +
              "' because the project symbol cache manifest is valid.",
          false);
      CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
      return;
    }
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: inspecting '" + fixtureLabel +
            "' because the project symbol cache manifest was not valid (" +
            std::string(symbol_cache::ValidationStatusName(cacheResult.status)) +
            ").",
        false);
  } else {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: inspecting '" + fixtureLabel +
            "' because the project symbol cache manifest could not be checked (" +
            (cacheError.empty() ? std::string("unknown cache error") : cacheError) +
            ").",
        false);
  }

  std::string inspectionError;
  symbol_preview::FixtureSymbolInspectionResult inspection;
  bool inspected = false;
  {
    symbols::ScopedFixtureSymbolPhase phase(&diagnostic.Timings(),
                                            symbols::FixtureSymbolPhase::Inspect);
    inspected = symbol_preview::InspectFixtureSymbolState(
        fixture, cfg.GetScene(), inspection, inspectionError);
  }
  if (!inspected) {
    if (!inspectionError.empty()) {
      ReportFixtureAutoUpdate(
          *this, consolePanel,
          "Fixture symbol auto-update: skipped '" + fixtureLabel +
              "' (inspection error: " + inspectionError + ").",
          false);
      fixtureSymbolAutoUpdateErrors.push_back("Skipped '" + fixtureLabel +
                                              "': " + inspectionError);
    }
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  if (!inspection.warningMessage.empty()) {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: warning for '" + fixtureLabel + "' (" +
            inspection.warningMessage + ").",
        false);
  }

  if (!inspection.requiresSymbolGeneration) {
    diagnostic.SetOutcome(symbols::FixtureSymbolOutcome::Skipped);
    if (hasCacheRequest) {
      cfg.GetSymbolCacheManifest().MarkFixtureSymbolsValid(cacheRequest);
      cfg.MarkDirty();
      ReportFixtureAutoUpdate(
          *this, consolePanel,
          "Fixture symbol auto-update: recorded valid project GDTF symbols for '" +
              fixtureLabel + "' in the project symbol cache manifest.",
          false);
    }
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  ReportFixtureAutoUpdate(*this, consolePanel,
                          "Fixture symbol auto-update: generating symbols for '" +
                              fixtureLabel +
                              "' because symbols were missing or invalid.",
                          false);

  Viewer2DOffscreenRenderer *offscreenRenderer = GetOffscreenRenderer();
  if (!offscreenRenderer) {
    fixtureSymbolAutoUpdateRunning = false;
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: stopped (offscreen renderer unavailable).",
                            false);
    fixtureSymbolAutoUpdateErrors.push_back(
        "Stopped: offscreen renderer unavailable");
    ReportFixtureAutoUpdate(*this, consolePanel,
                            BuildFixtureSymbolAutoUpdateSummary());
    if (fixtureSymbolAutoUpdateCompletionCallback) {
      auto completionCallback = fixtureSymbolAutoUpdateCompletionCallback;
      fixtureSymbolAutoUpdateCompletionCallback = nullptr;
      completionCallback();
    }
    return;
  }

  const std::string forcedFixtureColor = "#3FA9F5";
  tools::SceneModelSymbolCaptureOptions captureOptions;
  captureOptions.forcedFixtureColor = forcedFixtureColor;
  // Captured symbols must be orientation-neutral so per-instance rotation can be applied later in rendering/printing.
  captureOptions.alignToLocalAxes = true;
  captureOptions.timings = &diagnostic.Timings();

  auto capture = tools::CaptureSceneModelOrthographicSymbols(
      *offscreenRenderer, cfg,
      tools::SceneModelSymbolTarget{tools::SceneModelKind::Fixture, fixtureUuid},
      captureOptions);
  captureOptions.alignToLocalAxes = false;
  if (!capture.ok) {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to capture symbols for '" + fixtureLabel +
            "' (" + (capture.error.empty() ? std::string("unknown capture error")
                                            : capture.error) +
            ").",
        false);
    fixtureSymbolAutoUpdateErrors.push_back(
        "Capture failed for '" + fixtureLabel + "'");
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string calibrationError;
  bool calibrated = false;
  {
    symbols::ScopedFixtureSymbolPhase phase(
        &diagnostic.Timings(), symbols::FixtureSymbolPhase::Calibration);
    calibrated = capture.fixtureBoundsMm.valid
        ? tools::CalibrateFixtureSymbolsToPhysicalUnits(
              capture.fixtureBoundsMm, capture.symbols, calibrationError)
        : tools::CalibrateFixtureSymbolsToPhysicalUnits(
              cfg, fixtureUuid, capture.symbols, calibrationError);
  }
  if (!calibrated) {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to calibrate symbols for '" +
            fixtureLabel + "' (" +
            (calibrationError.empty() ? std::string("unknown calibration error")
                                      : calibrationError) +
            ").",
        false);
    fixtureSymbolAutoUpdateErrors.push_back(
        "Calibration failed for '" + fixtureLabel + "'");
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  symbol_preview::ApplySymbolsOptions applyOptions;
  applyOptions.updateSceneCopy = true;
  applyOptions.updateLibraryCopy = true;
  applyOptions.timings = &diagnostic.Timings();
  const symbol_preview::ApplySymbolsResult applyResult =
      symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          capture.symbols, fixtureUuid, applyOptions);
  if (applyResult.success && applyResult.sceneUpdated) {
    const std::string locationMessage =
        applyResult.libraryUpdated ? "scene and library" : "scene";

    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: symbols generated for '" +
                                fixtureLabel + "' and " + locationMessage +
                                " GDTF updated.",
                            false);
    cfg.MarkDirty();
    for (const std::string &warning : applyResult.warnings) {
      ReportFixtureAutoUpdate(
          *this, consolePanel,
          "Fixture symbol auto-update: library synchronization warning for '" +
              fixtureLabel + "' (" + warning + ").");
    }
    symbol_cache::ValidationRequest updatedCacheRequest;
    gui::fixtures::FixtureGdtfResolution updatedCacheResolution;
    std::string updatedCacheError;
    symbol_preview::FixtureSymbolInspectionResult updatedInspection;
    const auto updatedFixtureIt = cfg.GetScene().fixtures.find(fixtureUuid);
    const bool resolvedFinalScene =
        updatedFixtureIt != cfg.GetScene().fixtures.end() &&
        ResolveFixtureSymbolCacheRequest(
            updatedFixtureIt->second, cfg.GetScene(), updatedCacheRequest,
            updatedCacheResolution, updatedCacheError);
    std::error_code scenePathError;
    const bool finalSceneOwnershipConfirmed =
        resolvedFinalScene && !updatedCacheResolution.scenePath.empty() &&
        PathUtils::AreFilesystemPathsEquivalent(
            std::filesystem::path(updatedCacheResolution.scenePath),
            std::filesystem::path(applyResult.finalScenePath), scenePathError);
    if (resolvedFinalScene && !finalSceneOwnershipConfirmed) {
      std::ostringstream pathDiagnostic;
      pathDiagnostic
          << "resolved project archive '"
          << std::filesystem::path(updatedCacheResolution.scenePath).filename().string()
          << "' does not identify the validated archive '"
          << std::filesystem::path(applyResult.finalScenePath).filename().string()
          << "'";
      if (scenePathError)
        pathDiagnostic << " (filesystem check: " << scenePathError.message() << ")";
      updatedCacheError = pathDiagnostic.str();
    }
    if (finalSceneOwnershipConfirmed &&
        !applyResult.finalSceneFingerprint.empty())
      updatedCacheRequest.gdtfContentHash = applyResult.finalSceneFingerprint;
    if (finalSceneOwnershipConfirmed &&
        !applyResult.finalSceneFingerprint.empty() &&
        symbol_preview::InspectFixtureSymbolState(updatedFixtureIt->second,
                                                  cfg.GetScene(),
                                                  updatedInspection,
                                                  updatedCacheError) &&
        !updatedInspection.requiresSymbolGeneration) {
      cfg.GetSymbolCacheManifest().MarkFixtureSymbolsValid(updatedCacheRequest);
      cfg.MarkDirty();
    } else {
      ReportFixtureAutoUpdate(
          *this, consolePanel,
          "Fixture symbol auto-update: symbols generated for '" + fixtureLabel +
              "' but the project symbol cache manifest was not updated (" +
              (updatedCacheError.empty() ? std::string("symbol validation failed")
                                         : updatedCacheError) +
              ").",
          false);
    }

    if (fixtureSymbolAutoUpdateGeneratedTypeSet.insert(fixtureLabel).second)
      fixtureSymbolAutoUpdateGeneratedTypes.push_back(fixtureLabel);
    {
      symbols::ScopedFixtureSymbolPhase phase(&diagnostic.Timings(),
                                              symbols::FixtureSymbolPhase::Refresh);
      RefreshAfterFixtureSymbolUpdate();
    }
    diagnostic.SetOutcome(symbols::FixtureSymbolOutcome::Generated);
  } else {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to apply symbols for '" + fixtureLabel +
            "' (" + (applyResult.diagnostic.empty()
                           ? std::string("project-owned GDTF was not updated")
                           : applyResult.diagnostic) +
            ").",
        false);
    fixtureSymbolAutoUpdateErrors.push_back("Apply failed for '" + fixtureLabel +
                                            "'");
  }

  CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
}

void MainWindow::FlushPendingFixtureSymbolLibraryUpdates() {
  if (fixtureSymbolPendingLibrarySyncUuids.empty())
    return;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  std::string syncError;
  for (const std::string &fixtureUuid : fixtureSymbolPendingLibrarySyncUuids) {
    const auto fixtureIt = cfg.GetScene().fixtures.find(fixtureUuid);
    if (fixtureIt == cfg.GetScene().fixtures.end())
      continue;
    (void)symbol_preview::SyncFixtureGdtfToLibrary(fixtureIt->second, cfg.GetScene(),
                                                   syncError);
  }
  fixtureSymbolPendingLibrarySyncUuids.clear();
}



void MainWindow::RequestStartupSplashCompletion() {
  if (!startupSplashInitializationPending)
    return;
  startupSplashCloseRequested = true;
}

void MainWindow::OnStartupSplashCloseIdle(wxIdleEvent &event) {
  event.Skip();
  if (!startupSplashCloseRequested)
    return;

  startupSplashCloseRequested = false;
  CompleteStartupSplashInitialization();
}

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
