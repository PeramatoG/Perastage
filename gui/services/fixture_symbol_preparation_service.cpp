#include "services/fixture_symbol_preparation_service.h"

#include <filesystem>
#include <unordered_set>
#include <utility>

#include <wx/app.h>
#include <wx/event.h>

#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "guiconfigservices.h"
#include "mainwindow.h"
#include "symbols/PerastageSvgSymbol.h"
#include "symbols/fixture_symbol_availability.h"
#include "symbols/fixture_symbol_preparation_requests.h"
#include "symbols/fixture_symbol_resource_revision.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "fixture_symbol_source.h"
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
  window_.Bind(wxEVT_IDLE, &FixtureSymbolPreparationService::OnIdle, this);
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
  window_.Unbind(wxEVT_IDLE, &FixtureSymbolPreparationService::OnIdle, this);
  symbols::SetFixtureSymbolPreparationRequestHandler({});
  lifetime_.reset();
  coordinator_.BeginProjectEpoch();
}

// Invalidates previous work and optionally requests a post-stabilization scan.
void FixtureSymbolPreparationService::BeginProjectEpoch(bool scheduleScan) {
  epoch_ = coordinator_.BeginProjectEpoch();
  work_.clear();
  manualWork_.clear();
  fallbackDiagnostics_.clear();
  currentKey_.reset();
  scanFixtureUuids_.clear();
  scanKeys_.clear();
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
  for (const auto &[fixtureUuid, manualKey] : manualWork_) {
    (void)fixtureUuid;
    if (manualKey == key)
      return;
  }
  const auto source = symbols::InspectFixtureSymbolSource(
      key.effectiveGdtfPath, key.exactGdtfMode);
  if (source.source == symbols::FixtureSymbolSource::StoredGdtfSvg) {
    diagnostics::DiagnosticLogger::Info(
        "Fixture symbol preparation skipped because required SVGs are valid for resource '" +
        key.effectiveGdtfPath + "' mode '" + key.exactGdtfMode + "'.");
    return;
  }
  if (source.source == symbols::FixtureSymbolSource::PerastageFallback) {
    if (fallbackDiagnostics_.insert(key).second) {
      diagnostics::DiagnosticLogger::Info(
          "Fixture symbol generation skipped for resource '" +
          key.effectiveGdtfPath + "' mode '" + key.exactGdtfMode +
          "': " + source.diagnostic);
    }
    return;
  }
  const std::string fixtureUuid = FindFixtureUuid(key);
  if (fixtureUuid.empty())
    return;
  if (coordinator_.Request(key, key.effectiveGdtfPath, priority)) {
    work_[key].fixtureUuid = fixtureUuid;
    const auto &fixture = GetDefaultGuiConfigServices()
                              .LegacyConfigManager()
                              .GetScene()
                              .fixtures.at(fixtureUuid);
    work_[key].displayLabel = fixture.typeName.empty()
                                  ? std::filesystem::path(
                                        key.effectiveGdtfPath)
                                        .stem()
                                        .string()
                                  : fixture.typeName;
    std::string fingerprintError;
    work_[key].sourceFingerprint =
        symbol_cache::ComputeGdtfSemanticFingerprint(
            key.effectiveGdtfPath, fingerprintError);
    work_[key].sourceRevision =
        symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath);
  }
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
  manualWork_[fixtureUuid] = key;
  if (coordinator_.Find(key)) {
    coordinator_.Cancel(key, epoch_);
    if (currentKey_ && *currentKey_ == key)
      currentKey_.reset();
    work_.erase(key);
    UpdateStatus();
  }
  diagnostics::DiagnosticLogger::Info(
      "Fixture symbol automatic work paused for manual preview.");
}

// Resumes automatic eligibility when a manual preview ends without publication.
void FixtureSymbolPreparationService::CompleteManualFixture(
    const std::string &fixtureUuid, bool applied) {
  const auto manualIt = manualWork_.find(fixtureUuid);
  if (manualIt == manualWork_.end())
    return;
  const symbols::FixtureSymbolPreparationKey key = manualIt->second;
  manualWork_.erase(manualIt);
  if (!applied) {
    diagnostics::DiagnosticLogger::Info(
        "Fixture symbol manual preview ended without apply; automatic work is eligible again.");
    Request(key.effectiveGdtfPath, key.exactGdtfMode);
  }
}

// Scans unique resolved resources and exact modes after project stabilization.
void FixtureSymbolPreparationService::ScanCurrentProject() {
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)fixture;
    scanFixtureUuids_.push_back(uuid);
  }
  ScheduleNextStep();
}

// Requests one low-priority idle slice after normal GUI events are processed.
void FixtureSymbolPreparationService::ScheduleNextStep() {
  if (stepScheduled_)
    return;
  stepScheduled_ = true;
  wxWakeUpIdle();
}

// Executes at most one bounded automatic work slice per idle activation.
void FixtureSymbolPreparationService::OnIdle(wxIdleEvent &event) {
  if (!stepScheduled_)
    return;
  stepScheduled_ = false;
  if (!scanFixtureUuids_.empty()) {
    const std::string fixtureUuid = std::move(scanFixtureUuids_.front());
    scanFixtureUuids_.pop_front();
    const auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    const auto fixtureIt = scene.fixtures.find(fixtureUuid);
    if (fixtureIt != scene.fixtures.end()) {
      fixtures::FixtureGdtfResolution resolution;
      std::string error;
      if (fixtures::ResolveFixtureGdtfDeterministic(
              fixtureIt->second, scene, resolution, error, "symbol-scan")) {
        const symbols::FixtureSymbolPreparationKey key{
            CanonicalResourcePath(resolution.selectedPath),
            fixtureIt->second.gdtfMode};
        if (scanKeys_.insert(key).second)
          Request(key.effectiveGdtfPath, key.exactGdtfMode);
      }
    }
    if (!scanFixtureUuids_.empty())
      ScheduleNextStep();
  }
  RunNextStep();
  if (stepScheduled_)
    event.RequestMore();
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
      if (fixtureIt != scene.fixtures.end() &&
          fixtures::ResolveFixtureGdtfDeterministic(fixtureIt->second, scene,
                                                    resolution, resolutionError,
                                                    "symbol-reinspect")) {
        const auto source = symbols::InspectFixtureSymbolSource(
            resolution.selectedPath, currentKey_->exactGdtfMode);
        if (source.source !=
            symbols::FixtureSymbolSource::RenderableGdtfGeometry) {
          if (source.source == symbols::FixtureSymbolSource::PerastageFallback &&
              fallbackDiagnostics_.insert(*currentKey_).second) {
            diagnostics::DiagnosticLogger::Info(
                "Fixture symbol generation skipped during reinspection: " +
                source.diagnostic);
          }
          coordinator_.Skip(*currentKey_, epoch_);
          work_.erase(*currentKey_);
          currentKey_.reset();
          UpdateStatus();
          ScheduleNextStep();
          return;
        }
      }
    }
    if (!coordinator_.BeginCapture(*currentKey_, epoch_)) {
      currentKey_.reset();
      ScheduleNextStep();
      return;
    }
    diagnostics::DiagnosticLogger::Info(
        "Fixture symbol preparation started for resource '" +
        currentKey_->effectiveGdtfPath + "' mode '" +
        currentKey_->exactGdtfMode + "'.");
  }
  auto workIt = work_.find(*currentKey_);
  if (workIt == work_.end()) {
    FailCurrent("Fixture symbol preparation lost its runtime target.");
    return;
  }
  WorkContext &work = workIt->second;
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  if (!coordinator_.IsCurrent(epoch_) ||
      (work.stage != WorkStage::Finalizing &&
       !IsWorkIdentityCurrent(*currentKey_, work))) {
    FailCurrent("Fixture symbol preparation identity became stale.");
    return;
  }

  if (work.stage == WorkStage::Capturing) {
    Viewer2DOffscreenRenderer *renderer = window_.GetOffscreenRenderer();
    if (!renderer) {
      FailCurrent("Fixture symbol preparation could not acquire a renderer.");
      return;
    }
    tools::SceneModelSymbolCaptureOptions options;
    options.alignToLocalAxes = true;
    options.forcedFixtureColor = "#3FA9F5";
    if (work.bounds.valid)
      options.fixtureBoundsOverride = work.bounds;
    auto capture = tools::CaptureSceneModelOrthographicRenders(
        *renderer, cfg,
        {tools::SceneModelKind::Fixture, work.fixtureUuid}, options);
    if (!capture.ok) {
      FailCurrent(capture.error);
      return;
    }
    const symbol_cache::GdtfFileRevision capturedRevision =
        symbol_cache::ReadGdtfFileRevision(
            currentKey_->effectiveGdtfPath);
    if (!work.sourceRevision.metadataAvailable ||
        !capturedRevision.metadataAvailable ||
        capturedRevision != work.sourceRevision) {
      FailCurrent(
          "Fixture symbol source changed during capture; retry is required.");
      return;
    }
    work.bounds = capture.fixtureBoundsMm;
    work.renders = std::move(capture.renders);
    if (!coordinator_.CompleteCapture(*currentKey_, epoch_)) {
      FailCurrent("Fixture symbol capture state became stale.");
      return;
    }
    work.stage = WorkStage::Processing;
    UpdateStatus();
    ScheduleNextStep();
    return;
  }

  if (work.stage == WorkStage::Processing) {
    if (!work.processingSubmitted) {
      while (processingWorker_.TakeResult()) {
      }
      if (!processingWorker_.Submit(
              {epoch_, std::move(work.renders), work.bounds})) {
        ScheduleNextStep();
        return;
      }
      work.processingSubmitted = true;
      ScheduleNextStep();
      return;
    }
    auto processed = processingWorker_.TakeResult();
    if (!processed) {
      ScheduleNextStep();
      return;
    }
    if (processed->epoch != epoch_) {
      FailCurrent("Fixture symbol processing result became stale.");
      return;
    }
    if (!processed->ok) {
      FailCurrent(processed->error);
      return;
    }
    std::string calibrationError;
    const bool calibrated =
        processed->bounds.valid
            ? tools::CalibrateFixtureSymbolsToPhysicalUnits(
                  processed->bounds, processed->symbols,
                  calibrationError)
            : tools::CalibrateFixtureSymbolsToPhysicalUnits(
                  cfg, work.fixtureUuid, processed->symbols, calibrationError);
    if (!calibrated) {
      FailCurrent(calibrationError);
      return;
    }
    work.processedSymbols = std::move(processed->symbols);
    work.stage = WorkStage::Publishing;
    ScheduleNextStep();
    return;
  }

  if (work.stage == WorkStage::Publishing) {
    if (!coordinator_.IsCurrent(epoch_) ||
        !IsWorkIdentityCurrent(*currentKey_, work) ||
        !coordinator_.BeginPublishing(*currentKey_, epoch_)) {
      FailCurrent("Fixture symbol preparation became stale before publication.");
      return;
    }
    std::string fingerprintError;
    const symbol_cache::GdtfFileRevision currentRevision =
        symbol_cache::ReadGdtfFileRevision(currentKey_->effectiveGdtfPath);
    const bool revisionProvesUnchanged =
        work.sourceRevision.metadataAvailable &&
        currentRevision.metadataAvailable &&
        currentRevision == work.sourceRevision;
    const std::string currentFingerprint = revisionProvesUnchanged
        ? work.sourceFingerprint
        : symbol_cache::ComputeGdtfSemanticFingerprint(
              currentKey_->effectiveGdtfPath, fingerprintError);
    if (!work.sourceFingerprint.empty() &&
        currentFingerprint != work.sourceFingerprint) {
      const symbols::FixtureSymbolPreparationKey staleKey = *currentKey_;
      coordinator_.Cancel(staleKey, epoch_);
      work_.erase(staleKey);
      currentKey_.reset();
      diagnostics::DiagnosticLogger::Info(
          "Fixture symbol preparation source changed; stale work was discarded.");
      Request(staleKey.effectiveGdtfPath, staleKey.exactGdtfMode);
      return;
    }
    if (symbol_cache::InspectFixtureSymbolAvailability(
            currentKey_->effectiveGdtfPath).storedSvgUsable) {
      coordinator_.Complete(*currentKey_, epoch_, true);
    } else {
      const auto apply = symbol_preview::ApplySymbolsToFixtureGdtfWithResult(
          work.processedSymbols, work.fixtureUuid);
      if (!coordinator_.IsCurrent(epoch_) || !apply.success) {
        FailCurrent(apply.diagnostic.empty()
                        ? "Fixture symbol publication was rejected as stale."
                        : apply.diagnostic);
        return;
      }
      work.sceneUpdated = apply.sceneUpdated;
      coordinator_.Complete(*currentKey_, epoch_, true);
    }
    work.stage = WorkStage::Finalizing;
    ScheduleNextStep();
    return;
  }

  if (work.sceneUpdated) {
    cfg.MarkDirty();
    window_.RefreshAfterFixtureSymbolUpdate();
  }
  diagnostics::DiagnosticLogger::Info(
      "Fixture symbol preparation completed for resource '" +
      currentKey_->effectiveGdtfPath + "' mode '" +
      currentKey_->exactGdtfMode + "'.");
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
  const WorkContext *statusWork = nullptr;
  const symbols::FixtureSymbolPreparationKey *statusKey = nullptr;
  if (currentKey_) {
    const auto it = work_.find(*currentKey_);
    if (it != work_.end()) {
      statusWork = &it->second;
      statusKey = &*currentKey_;
    }
  }
  std::optional<symbols::FixtureSymbolPreparationJob> next;
  if (!statusWork) {
    next = coordinator_.NextQueued();
    if (next) {
      const auto it = work_.find(next->key);
      if (it != work_.end()) {
        statusWork = &it->second;
        statusKey = &next->key;
      }
    }
  }
  std::string context = statusWork ? statusWork->displayLabel : "fixture resource";
  if (statusKey && !statusKey->exactGdtfMode.empty())
    context += " [" + statusKey->exactGdtfMode + "]";
  std::string phase = "Preparing";
  if (statusWork) {
    switch (statusWork->stage) {
    case WorkStage::Capturing:
      phase = "Capturing fixture views";
      break;
    case WorkStage::Processing:
      phase = "Processing";
      break;
    case WorkStage::Publishing:
      phase = "Publishing";
      break;
    case WorkStage::Finalizing:
      phase = "Refreshing";
      break;
    }
  }
  window_.SetStatusText("Preparing fixture symbols: " + context + " - " +
                            phase + " - " + std::to_string(pending) +
                            " pending",
                        0);
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

// Revalidates that a job still targets the same current-project resource and mode.
bool FixtureSymbolPreparationService::IsWorkIdentityCurrent(
    const symbols::FixtureSymbolPreparationKey &key,
    const WorkContext &work) const {
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto fixtureIt = scene.fixtures.find(work.fixtureUuid);
  if (fixtureIt == scene.fixtures.end() ||
      fixtureIt->second.gdtfMode != key.exactGdtfMode)
    return false;
  fixtures::FixtureGdtfResolution resolution;
  std::string error;
  return fixtures::ResolveFixtureGdtfDeterministic(
             fixtureIt->second, scene, resolution, error, "symbol-identity") &&
         CanonicalResourcePath(resolution.selectedPath) == key.effectiveGdtfPath;
}

} // namespace gui
