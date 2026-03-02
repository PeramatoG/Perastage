#include "mainwindow.h"

#include <string>

#include "configmanager.h"
#include "consolepanel.h"
#include "fixture.h"
#include "guiconfigservices.h"
#include "opaque_pass_utils.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "windows/symbol_fixture_applier.h"

namespace {

std::string BuildFixtureAutoUpdateKey(const Fixture &fixture) {
  if (!fixture.typeName.empty())
    return fixture.typeName;
  if (!fixture.gdtfSpec.empty())
    return NormalizeModelKey(fixture.gdtfSpec);
  return {};
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

  if (fixtureSymbolAutoUpdateQueue.empty())
    return;

  fixtureSymbolAutoUpdateRunning = true;
  CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
}

void MainWindow::ProcessNextFixtureSymbolAutoUpdate() {
  if (!fixtureSymbolAutoUpdateRunning)
    return;

  if (fixtureSymbolAutoUpdateQueue.empty()) {
    fixtureSymbolAutoUpdateRunning = false;
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

  const std::string key = BuildFixtureAutoUpdateKey(fixtureIt->second);
  if (key.empty() || !fixtureSymbolAutoUpdateProcessedKeys.insert(key).second) {
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string inspectionError;
  symbol_preview::FixtureSymbolInspectionResult inspection;
  if (!symbol_preview::InspectFixtureSymbolState(fixtureIt->second, cfg.GetScene(),
                                                 inspection, inspectionError)) {
    if (consolePanel && !inspectionError.empty()) {
      consolePanel->AppendMessage(
          "Fixture symbol auto-update skipped: " + wxString::FromUTF8(inspectionError));
    }
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  if (!inspection.requiresSymbolGeneration) {
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  Viewer2DOffscreenRenderer *offscreenRenderer = GetOffscreenRenderer();
  if (!offscreenRenderer) {
    fixtureSymbolAutoUpdateRunning = false;
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
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string calibrationError;
  if (!tools::CalibrateFixtureSymbolsToPhysicalUnits(
          cfg, fixtureUuid, capture.symbols, calibrationError)) {
    CallAfter([this]() { ProcessNextFixtureSymbolAutoUpdate(); });
    return;
  }

  std::string applyError;
  symbol_preview::ApplySymbolsOptions applyOptions;
  applyOptions.updateSceneCopy = false;
  applyOptions.updateLibraryCopy = true;
  if (symbol_preview::ApplySymbolsToFixtureGdtf(capture.symbols, fixtureUuid,
                                                applyError, applyOptions)) {
    fixtureSymbolPendingLibrarySyncUuids.insert(fixtureUuid);
    RefreshAfterFixtureSymbolUpdate();
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
