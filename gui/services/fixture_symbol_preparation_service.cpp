#include "services/fixture_symbol_preparation_service.h"

#include <filesystem>
#include <utility>

#include <wx/app.h>

#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "guiconfigservices.h"
#include "mainwindow.h"
#include "symbols/PerastageSvgSymbol.h"
#include "symbols/fixture_symbol_preparation_requests.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "viewer2doffscreenrenderer.h"
#include "windows/symbol_fixture_applier.h"

namespace gui {
namespace {

// Returns a stable physical spelling for a resolved GDTF resource path.
std::string CanonicalResourcePath(const std::string &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return error ? std::filesystem::path(path).lexically_normal().string()
               : canonical.string();
}

} // namespace

// Installs the runtime request bridge used by immediate rendering fallbacks.
FixtureSymbolPreparationService::FixtureSymbolPreparationService(
    MainWindow &window)
    : window_(window), epoch_(coordinator_.BeginProjectEpoch()) {
  const std::weak_ptr<int> weakLifetime = lifetime_;
  symbols::SetFixtureSymbolPreparationRequestHandler(
      [this, weakLifetime](const std::string &path, const std::string &mode) {
        if (!weakLifetime.lock())
          return;
        window_.CallAfter([this, weakLifetime, path, mode]() {
          if (weakLifetime.lock())
            Request(path, mode);
        });
      });
}

// Disconnects fallback requests and invalidates all delayed service callbacks.
FixtureSymbolPreparationService::~FixtureSymbolPreparationService() {
  symbols::SetFixtureSymbolPreparationRequestHandler({});
  lifetime_.reset();
  coordinator_.BeginProjectEpoch();
}

// Invalidates previous work and optionally requests a post-stabilization scan.
void FixtureSymbolPreparationService::BeginProjectEpoch(bool scheduleScan) {
  epoch_ = coordinator_.BeginProjectEpoch();
  work_.clear();
  currentKey_.reset();
  stepScheduled_ = false;
  UpdateStatus();
  if (scheduleScan)
    ScheduleScan();
}

// Posts a lifetime-safe scan after the current project operation returns.
void FixtureSymbolPreparationService::ScheduleScan() {
  const std::weak_ptr<int> weakLifetime = lifetime_;
  const std::uint64_t requestedEpoch = epoch_;
  window_.CallAfter([this, weakLifetime, requestedEpoch]() {
    if (!weakLifetime.lock() || requestedEpoch != epoch_)
      return;
    ScanCurrentProject();
  });
}

// Coalesces one physical-resource and exact-mode preparation request.
void FixtureSymbolPreparationService::Request(
    const std::string &effectiveGdtfPath, const std::string &exactGdtfMode,
    symbols::FixtureSymbolPreparationPriority priority) {
  if (effectiveGdtfPath.empty())
    return;
  const symbols::FixtureSymbolPreparationKey key{
      CanonicalResourcePath(effectiveGdtfPath), exactGdtfMode};
  RequiredFixtureSvgSetInspection inspection;
  if (InspectRequiredFixtureSvgSet(key.effectiveGdtfPath, inspection) &&
      inspection.usable)
    return;
  const std::string fixtureUuid = FindFixtureUuid(key);
  if (fixtureUuid.empty())
    return;
  if (coordinator_.Request(key, key.effectiveGdtfPath, priority))
    work_[key].fixtureUuid = fixtureUuid;
  UpdateStatus();
  ScheduleNextStep();
}

// Promotes matching queued automatic work before the manual preview workflow.
void FixtureSymbolPreparationService::PromoteManualFixture(
    const std::string &fixtureUuid) {
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto fixtureIt = scene.fixtures.find(fixtureUuid);
  if (fixtureIt == scene.fixtures.end())
    return;
  fixtures::FixtureGdtfResolution resolution;
  std::string error;
  if (!fixtures::ResolveFixtureGdtfDeterministic(
          fixtureIt->second, scene, resolution, error, "manual-symbol"))
    return;
  const symbols::FixtureSymbolPreparationKey key{
      CanonicalResourcePath(resolution.selectedPath),
      fixtureIt->second.gdtfMode};
  if (coordinator_.Find(key)) {
    coordinator_.Fail(key, epoch_);
    if (currentKey_ && *currentKey_ == key)
      currentKey_.reset();
    work_.erase(key);
    UpdateStatus();
  }
}

// Scans unique resolved resources and exact modes after project stabilization.
void FixtureSymbolPreparationService::ScanCurrentProject() {
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    fixtures::FixtureGdtfResolution resolution;
    std::string error;
    if (!fixtures::ResolveFixtureGdtfDeterministic(fixture, scene, resolution,
                                                   error, "symbol-scan"))
      continue;
    Request(resolution.selectedPath, fixture.gdtfMode);
  }
}

// Posts exactly one capture, processing, or publication slice to the GUI loop.
void FixtureSymbolPreparationService::ScheduleNextStep() {
  if (stepScheduled_)
    return;
  stepScheduled_ = true;
  const std::weak_ptr<int> weakLifetime = lifetime_;
  const std::uint64_t requestedEpoch = epoch_;
  window_.CallAfter([this, weakLifetime, requestedEpoch]() {
    if (!weakLifetime.lock())
      return;
    stepScheduled_ = false;
    if (requestedEpoch != epoch_)
      return;
    RunNextStep();
  });
}

// Advances the active automatic job by one bounded production operation.
void FixtureSymbolPreparationService::RunNextStep() {
  if (!currentKey_) {
    const auto next = coordinator_.NextQueued();
    if (!next) {
      UpdateStatus();
      return;
    }
    currentKey_ = next->key;
    const auto workIt = work_.find(*currentKey_);
    const auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    if (workIt != work_.end()) {
      const auto fixtureIt = scene.fixtures.find(workIt->second.fixtureUuid);
      fixtures::FixtureGdtfResolution resolution;
      std::string resolutionError;
      RequiredFixtureSvgSetInspection inspection;
      if (fixtureIt != scene.fixtures.end() &&
          fixtures::ResolveFixtureGdtfDeterministic(fixtureIt->second, scene,
                                                    resolution, resolutionError,
                                                    "symbol-reinspect") &&
          InspectRequiredFixtureSvgSet(resolution.selectedPath, inspection) &&
          inspection.usable) {
        coordinator_.Skip(*currentKey_, epoch_);
        currentKey_.reset();
        UpdateStatus();
        ScheduleNextStep();
        return;
      }
    }
    if (!coordinator_.BeginCapture(*currentKey_, epoch_)) {
      currentKey_.reset();
      ScheduleNextStep();
      return;
    }
  }
  auto workIt = work_.find(*currentKey_);
  if (workIt == work_.end()) {
    FailCurrent("Fixture symbol preparation lost its runtime target.");
    return;
  }
  WorkContext &work = workIt->second;
  Viewer2DOffscreenRenderer *renderer = window_.GetOffscreenRenderer();
  if (!renderer) {
    FailCurrent("Fixture symbol preparation could not acquire a renderer.");
    return;
  }
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();

  if (work.nextCaptureStep <= symbols::FixtureSymbolCapturePlan().size()) {
    tools::SceneModelSymbolCaptureOptions options;
    options.alignToLocalAxes = true;
    options.forcedFixtureColor = "#3FA9F5";
    auto capture = tools::CaptureSceneModelOrthographicStep(
        *renderer, cfg, {tools::SceneModelKind::Fixture, work.fixtureUuid},
        work.nextCaptureStep, options);
    if (!capture.ok) {
      FailCurrent(capture.error);
      return;
    }
    if (capture.fixtureBoundsMm.valid)
      work.bounds = capture.fixtureBoundsMm;
    if (capture.image) {
      work.renders.push_back(std::move(*capture.image));
      coordinator_.CompleteCaptureStep(*currentKey_, epoch_);
    }
    ++work.nextCaptureStep;
    UpdateStatus();
    ScheduleNextStep();
    return;
  }

  auto processed = tools::ProcessSceneModelOrthographicRenders(
      std::move(work.renders), work.bounds);
  if (!processed.ok) {
    FailCurrent(processed.error);
    return;
  }
  std::string calibrationError;
  const bool calibrated =
      processed.fixtureBoundsMm.valid
          ? tools::CalibrateFixtureSymbolsToPhysicalUnits(
                processed.fixtureBoundsMm, processed.symbols, calibrationError)
          : tools::CalibrateFixtureSymbolsToPhysicalUnits(
                cfg, work.fixtureUuid, processed.symbols, calibrationError);
  if (!calibrated) {
    FailCurrent(calibrationError);
    return;
  }
  if (!coordinator_.IsCurrent(epoch_) ||
      !coordinator_.BeginPublishing(*currentKey_, epoch_)) {
    FailCurrent("Fixture symbol preparation became stale before publication.");
    return;
  }

  RequiredFixtureSvgSetInspection currentInspection;
  if (InspectRequiredFixtureSvgSet(currentKey_->effectiveGdtfPath,
                                   currentInspection) &&
      currentInspection.usable) {
    coordinator_.Complete(*currentKey_, epoch_, true);
  } else {
    const auto apply = symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
        processed.symbols, work.fixtureUuid);
    if (!coordinator_.IsCurrent(epoch_) || !apply.success) {
      FailCurrent(apply.diagnostic.empty()
                      ? "Fixture symbol publication was rejected as stale."
                      : apply.diagnostic);
      return;
    }
    coordinator_.Complete(*currentKey_, epoch_, true);
    if (apply.sceneUpdated) {
      cfg.MarkDirty();
      window_.RefreshAfterFixtureSymbolUpdate();
    }
    diagnostics::DiagnosticLogger::Info(
        "Fixture symbol preparation completed for one resource.");
  }
  currentKey_.reset();
  UpdateStatus();
  ScheduleNextStep();
}

// Leaves fallback rendering active and suppresses retries for this epoch.
void FixtureSymbolPreparationService::FailCurrent(
    const std::string &diagnostic) {
  if (currentKey_)
    coordinator_.Fail(*currentKey_, epoch_);
  diagnostics::DiagnosticLogger::Warning("Fixture symbol preparation failed: " +
                                         diagnostic);
  currentKey_.reset();
  UpdateStatus();
  ScheduleNextStep();
}

// Displays concise aggregate non-modal progress and clears it when idle.
void FixtureSymbolPreparationService::UpdateStatus() {
  const std::size_t pending = coordinator_.PendingCount();
  if (pending == 0) {
    window_.SetStatusText("Ready", 0);
    return;
  }
  window_.SetStatusText(
      "Preparing fixture symbols: " + std::to_string(pending) + " pending", 0);
}

// Finds a current-project fixture that owns the exact preparation identity.
std::string FixtureSymbolPreparationService::FindFixtureUuid(
    const symbols::FixtureSymbolPreparationKey &key) const {
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  std::string selected;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (fixture.gdtfMode != key.exactGdtfMode)
      continue;
    fixtures::FixtureGdtfResolution resolution;
    std::string error;
    if (!fixtures::ResolveFixtureGdtfDeterministic(fixture, scene, resolution,
                                                   error, "symbol-request") ||
        CanonicalResourcePath(resolution.selectedPath) != key.effectiveGdtfPath)
      continue;
    if (selected.empty() || uuid < selected)
      selected = uuid;
  }
  return selected;
}

} // namespace gui
