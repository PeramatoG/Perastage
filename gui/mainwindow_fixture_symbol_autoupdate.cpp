#include "mainwindow.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "configmanager.h"
#include "consolepanel.h"
#include "fixture.h"
#include "guiconfigservices.h"
#include "opaque_pass_utils.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "windows/symbol_fixture_applier.h"

#include <wx/app.h>
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
  if (!fixture.typeName.empty())
    return fixture.typeName;
  if (!fixture.instanceName.empty())
    return fixture.instanceName;
  if (!fixture.gdtfSpec.empty())
    return std::filesystem::path(fixture.gdtfSpec).filename().string();
  return "unknown fixture";
}


std::string BuildGeneratedTypesSummary(
    const std::unordered_set<std::string> &generatedTypes) {
  if (generatedTypes.empty())
    return "none";

  std::vector<std::string> types(generatedTypes.begin(), generatedTypes.end());
  std::sort(types.begin(), types.end());

  std::ostringstream out;
  for (size_t i = 0; i < types.size(); ++i) {
    if (i > 0)
      out << ", ";
    out << "'" << types[i] << "'";
  }
  return out.str();
}
void ReportFixtureAutoUpdate(MainWindow &window, ConsolePanel *console,
                             const std::string &message,
                             bool appendToConsole = true) {
  static const int kStatusClearTimerId = wxWindow::NewControlId();
  static std::unordered_map<MainWindow *, std::unique_ptr<wxTimer>> timers;
  static std::unordered_map<MainWindow *, std::string> pendingStatusText;

  auto timerIt = timers.find(&window);
  if (timerIt == timers.end()) {
    auto timer = std::make_unique<wxTimer>(&window, kStatusClearTimerId);
    window.Bind(
        wxEVT_TIMER,
        [windowPtr = &window](wxTimerEvent &) {
          auto pendingIt = pendingStatusText.find(windowPtr);
          if (pendingIt == pendingStatusText.end())
            return;
          if (windowPtr->GetStatusBar() &&
              windowPtr->GetStatusBar()->GetStatusText(0) ==
                  wxString::FromUTF8(pendingIt->second)) {
            windowPtr->SetStatusText("", 0);
          }
          pendingStatusText.erase(pendingIt);
        },
        kStatusClearTimerId);
    timerIt = timers.emplace(&window, std::move(timer)).first;
  }

  window.SetStatusText(wxString::FromUTF8(message), 0);
  pendingStatusText[&window] = message;
  timerIt->second->StartOnce(10000);

  if (console && appendToConsole)
    console->AppendMessage(wxString::FromUTF8(message));
}


} // namespace

void MainWindow::StartFixtureSymbolAutoUpdateForLoadedScene() {
  fixtureSymbolAutoUpdateQueue.clear();
  fixtureSymbolAutoUpdateProcessedKeys.clear();
  fixtureSymbolPendingLibrarySyncUuids.clear();
  fixtureSymbolAutoUpdateRunning = false;
  fixtureSymbolAutoUpdateWorkerBusy = false;
  fixtureSymbolAutoUpdateGeneratedTypes.clear();
  fixtureSymbolAutoUpdateGeneratedCount = 0;
  fixtureSymbolAutoUpdateFailedCount = 0;
  fixtureSymbolAutoUpdateSkippedCount = 0;

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
  ScheduleNextFixtureSymbolAutoUpdate(700);
}


void MainWindow::ScheduleNextFixtureSymbolAutoUpdate(int delayMs) {
  static const int kProcessNextTimerId = wxWindow::NewControlId();
  static std::unordered_map<MainWindow *, std::unique_ptr<wxTimer>> timers;

  auto timerIt = timers.find(this);
  if (timerIt == timers.end()) {
    auto timer = std::make_unique<wxTimer>(this, kProcessNextTimerId);
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent &) { ProcessNextFixtureSymbolAutoUpdate(); },
        kProcessNextTimerId);
    timerIt = timers.emplace(this, std::move(timer)).first;
  }

  timerIt->second->StartOnce(delayMs);
}

void MainWindow::ProcessNextFixtureSymbolAutoUpdate() {
  if (!fixtureSymbolAutoUpdateRunning)
    return;
  if (fixtureSymbolAutoUpdateWorkerBusy)
    return;

  if (fixtureSymbolAutoUpdateQueue.empty()) {
    fixtureSymbolAutoUpdateRunning = false;
    std::ostringstream summary;
    summary << "Fixture symbol auto-update: completed. generated="
            << fixtureSymbolAutoUpdateGeneratedCount
            << ", failed=" << fixtureSymbolAutoUpdateFailedCount
            << ", skipped=" << fixtureSymbolAutoUpdateSkippedCount
            << ", generated types="
            << BuildGeneratedTypesSummary(fixtureSymbolAutoUpdateGeneratedTypes)
            << ".";
    if (consolePanel)
      consolePanel->AppendMessage(wxString::FromUTF8(summary.str()));
    RefreshAfterFixtureSymbolUpdate();
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: completed.", false);
    return;
  }

  const std::string fixtureUuid = fixtureSymbolAutoUpdateQueue.back();
  fixtureSymbolAutoUpdateQueue.pop_back();

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto fixtureIt = cfg.GetScene().fixtures.find(fixtureUuid);
  if (fixtureIt == cfg.GetScene().fixtures.end()) {
    ScheduleNextFixtureSymbolAutoUpdate();
    return;
  }

  const Fixture &fixture = fixtureIt->second;
  const std::string fixtureLabel = BuildFixtureLabel(fixture);
  const std::string key = BuildFixtureAutoUpdateKey(fixture);
  if (key.empty() || !fixtureSymbolAutoUpdateProcessedKeys.insert(key).second) {
    ScheduleNextFixtureSymbolAutoUpdate();
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
              "' (inspection error: " + inspectionError + ").", false);
      ++fixtureSymbolAutoUpdateSkippedCount;
    }
    ScheduleNextFixtureSymbolAutoUpdate();
    return;
  }

  if (!inspection.requiresSymbolGeneration) {
    ++fixtureSymbolAutoUpdateSkippedCount;
    ScheduleNextFixtureSymbolAutoUpdate();
    return;
  }

  ReportFixtureAutoUpdate(*this, consolePanel,
                          "Fixture symbol auto-update: generating symbols for '" +
                              fixtureLabel + "'.", false);

  Viewer2DOffscreenRenderer *offscreenRenderer = GetOffscreenRenderer();
  if (!offscreenRenderer) {
    fixtureSymbolAutoUpdateRunning = false;
    ReportFixtureAutoUpdate(*this, consolePanel,
                            "Fixture symbol auto-update: stopped (offscreen renderer unavailable).", false);
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
    ++fixtureSymbolAutoUpdateFailedCount;
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to capture symbols for '" + fixtureLabel +
            "' (" + (capture.error.empty() ? std::string("unknown capture error")
                                            : capture.error) +
            ").", false);
    ScheduleNextFixtureSymbolAutoUpdate();
    return;
  }

  std::string calibrationError;
  if (!tools::CalibrateFixtureSymbolsToPhysicalUnits(
          cfg, fixtureUuid, capture.symbols, calibrationError)) {
    ++fixtureSymbolAutoUpdateFailedCount;
    ReportFixtureAutoUpdate(
        *this, consolePanel,
        "Fixture symbol auto-update: failed to calibrate symbols for '" +
            fixtureLabel + "' (" +
            (calibrationError.empty() ? std::string("unknown calibration error")
                                      : calibrationError) +
            ").", false);
    ScheduleNextFixtureSymbolAutoUpdate();
    return;
  }

  fixtureSymbolAutoUpdateWorkerBusy = true;
  const Fixture fixtureCopy = fixture;
  const MvrScene sceneSnapshot = cfg.GetScene();
  const auto capturedSymbols = capture.symbols;
  const auto inspectionSnapshot = inspection;

  std::thread([this, capturedSymbols, fixtureCopy, sceneSnapshot,
               inspectionSnapshot, fixtureLabel]() {
    std::string applyError;
    symbol_preview::ApplySymbolsOptions applyOptions;
    applyOptions.updateSceneCopy = true;
    applyOptions.updateLibraryCopy = true;
    const bool applied = symbol_preview::ApplySymbolsToFixtureGdtfForFixture(
        capturedSymbols, fixtureCopy, sceneSnapshot, applyError, applyOptions);

    wxTheApp->CallAfter([this, applied, applyError, inspectionSnapshot,
                         fixtureLabel, fixtureType = fixtureCopy.typeName]() {
      if (!fixtureSymbolAutoUpdateRunning) {
        fixtureSymbolAutoUpdateWorkerBusy = false;
        return;
      }

      if (applied) {
        std::string locationMessage = "scene";
        if (!inspectionSnapshot.scenePath.empty() &&
            !inspectionSnapshot.libraryPath.empty())
          locationMessage = "scene and library";
        else if (!inspectionSnapshot.libraryPath.empty())
          locationMessage = "library";

        ReportFixtureAutoUpdate(
            *this, consolePanel,
            "Fixture symbol auto-update: symbols generated for '" + fixtureLabel +
                "' and " + locationMessage + " GDTF updated.",
            false);
        ++fixtureSymbolAutoUpdateGeneratedCount;
        fixtureSymbolAutoUpdateGeneratedTypes.insert(
            fixtureType.empty() ? fixtureLabel : fixtureType);
        RefreshAfterFixtureSymbolUpdate();
      } else {
        ++fixtureSymbolAutoUpdateFailedCount;
        ReportFixtureAutoUpdate(
            *this, consolePanel,
            "Fixture symbol auto-update: failed to apply symbols for '" +
                fixtureLabel + "' (" +
                (applyError.empty() ? std::string("unknown apply error")
                                    : applyError) +
                ").",
            false);
      }

      fixtureSymbolAutoUpdateWorkerBusy = false;
      ScheduleNextFixtureSymbolAutoUpdate();
    });
  }).detach();


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
