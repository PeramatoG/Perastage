#include "mainwindow.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "configmanager.h"
#include "consolepanel.h"
#include "fixture.h"
#include "guiconfigservices.h"
#include "opaque_pass_utils.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "windows/symbol_fixture_applier.h"

#include <wx/timer.h>

namespace {

std::string BuildFixtureAutoUpdateKey(const Fixture &fixture) {
  if (!fixture.typeName.empty())
    return fixture.typeName;
  if (!fixture.gdtfSpec.empty())
    return NormalizeModelKey(fixture.gdtfSpec);
  return {};
}

std::string BuildFixtureLabel(const Fixture &fixture) {
  if (!fixture.instanceName.empty())
    return fixture.instanceName;
  if (!fixture.typeName.empty())
    return fixture.typeName;
  if (!fixture.gdtfSpec.empty())
    return std::filesystem::path(fixture.gdtfSpec).filename().string();
  return "unknown fixture";
}

void ReportFixtureAutoUpdate(MainWindow &window, ConsolePanel *console,
                             const std::string &message) {
  static const int kStatusClearTimerId = wxWindow::NewControlId();
  static std::unordered_map<MainWindow *, std::unique_ptr<wxTimer>> timers;
  static std::unordered_map<MainWindow *, std::string> pendingStatusText;

  auto timerIt = timers.find(&window);
  if (timerIt == timers.end()) {
    auto timer = std::make_unique<wxTimer>(&window, kStatusClearTimerId);
    window.Bind(
        wxEVT_TIMER,
        [&window, &pendingStatusText](wxTimerEvent &) {
          auto pendingIt = pendingStatusText.find(&window);
          if (pendingIt == pendingStatusText.end())
            return;
          if (window.GetStatusBar() &&
              window.GetStatusBar()->GetStatusText(0) ==
                  wxString::FromUTF8(pendingIt->second)) {
            window.SetStatusText("", 0);
          }
          pendingStatusText.erase(pendingIt);
        },
        kStatusClearTimerId);
    timerIt = timers.emplace(&window, std::move(timer)).first;
  }

  window.SetStatusText(wxString::FromUTF8(message), 0);
  pendingStatusText[&window] = message;
  timerIt->second->StartOnce(10000);

  if (console)
    console->AppendMessage(wxString::FromUTF8(message));
}

} // namespace

void MainWindow::StartFixtureSymbolAutoUpdateForLoadedScene() {
  fixtureSymbolAutoUpdateQueue.clear();
  fixtureSymbolAutoUpdateProcessedKeys.clear();
  fixtureSymbolPendingLibrarySyncUuids.clear();
  fixtureSymbolAutoUpdateRunning = false;

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  for (const auto &[uuid, fixture] : cfg.GetScene().fixtures) {
    const std::string key = BuildFixtureAutoUpdateKey(fixture);
    if (key.empty())
      continue;
    if (!fixtureSymbolAutoUpdateProcessedKeys.insert(key).second)
      continue;
    fixtureSymbolAutoUpdateQueue.push_back(uuid);
  }

  fixtureSymbolAutoUpdateProcessedKeys.clear();

  if (fixtureSymbolAutoUpdateQueue.empty()) {
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: no fixtures queued.");
    return;
  }

  fixtureSymbolAutoUpdateRunning = true;
  ReportFixtureAutoUpdate(*this, consolePanel,
                          "Fixture symbol auto-update: started.");
  CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
}

void MainWindow::ProcessNextFixtureSymbolAutoUpdate() {
  if (!fixtureSymbolAutoUpdateRunning)
    return;

  if (fixtureSymbolAutoUpdateQueue.empty()) {
    fixtureSymbolAutoUpdateRunning = false;
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: completed.");
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
  const std::string key = BuildFixtureAutoUpdateKey(fixture);
  if (key.empty() || !fixtureSymbolAutoUpdateProcessedKeys.insert(key).second) {
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string inspectionError;
  symbol_preview::FixtureSymbolInspectionResult inspection;
  if (!symbol_preview::InspectFixtureSymbolState(fixture, cfg.GetScene(), inspection,
                                                 inspectionError)) {
    if (!inspectionError.empty()) {
      ReportFixtureAutoUpdate(
          *this, consolePanel,
          "Fixture symbol auto-update: skipped '" + fixtureLabel +
              "' (inspection error: " + inspectionError + ").");
    }
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  if (!inspection.requiresSymbolGeneration) {
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  ReportFixtureAutoUpdate(*this, consolePanel,
                          "Fixture symbol auto-update: generating symbols for '" +
                              fixtureLabel + "'.");

  Viewer2DOffscreenRenderer *offscreenRenderer = GetOffscreenRenderer();
  if (!offscreenRenderer) {
    fixtureSymbolAutoUpdateRunning = false;
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: stopped (offscreen renderer unavailable)." );
    return;
  }

  const std::string forcedFixtureColor = "#3FA9F5";
  tools::SceneModelSymbolCaptureOptions captureOptions;
  captureOptions.forcedFixtureColor = forcedFixtureColor;

  auto capture = tools::CaptureSceneModelOrthographicSymbols(
      *offscreenRenderer, cfg,
      tools::SceneModelSymbolTarget{tools::SceneModelKind::Fixture, fixtureUuid},
      captureOptions);
  if (!capture.ok) {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to capture symbols for '" + fixtureLabel +
            "' (" + (capture.error.empty() ? std::string("unknown capture error")
                                            : capture.error) +
            ").");
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string calibrationError;
  if (!tools::CalibrateFixtureSymbolsToPhysicalUnits(
          cfg, fixtureUuid, capture.symbols, calibrationError)) {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to calibrate symbols for '" +
            fixtureLabel + "' (" +
            (calibrationError.empty() ? std::string("unknown calibration error")
                                      : calibrationError) +
            ").");
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string applyError;
  symbol_preview::ApplySymbolsOptions applyOptions;
  applyOptions.updateSceneCopy = true;
  applyOptions.updateLibraryCopy = true;
  if (symbol_preview::ApplySymbolsToFixtureGdtf(capture.symbols, fixtureUuid,
                                                applyError, applyOptions)) {
    std::string locationMessage = "scene";
    if (!inspection.scenePath.empty() && !inspection.libraryPath.empty())
      locationMessage = "scene and library";
    else if (!inspection.libraryPath.empty())
      locationMessage = "library";

    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: symbols generated for '" +
                                fixtureLabel + "' and " + locationMessage +
                                " GDTF updated.");
    RefreshAfterFixtureSymbolUpdate();
  } else {
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to apply symbols for '" + fixtureLabel +
            "' (" + (applyError.empty() ? std::string("unknown apply error")
                                         : applyError) +
            ").");
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
