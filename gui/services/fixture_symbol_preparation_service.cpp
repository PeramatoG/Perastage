#include "symbols/fixture_symbol_operation_counters.h"
#include "services/fixture_symbol_preparation_service.h"

#include <filesystem>
#include <unordered_set>
#include <utility>

#include <wx/app.h>
#include <wx/event.h>
#include <wx/statusbr.h>

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
#include "gdtf_mutation_policy.h"
#include "symbols/fixture_symbol_application.h"

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
    : window_(window),
      notifications_(std::make_shared<FixtureSymbolPreparationNotifications>(
          [this](const auto &key) { EnqueueRequest(key); },
          [this]() {
            workerReady_ = true;
            ScheduleNextStep();
          })),
      processingWorker_([notifications = notifications_]() {
        notifications->Completed();
      }), epoch_(coordinator_.BeginProjectEpoch()) {
  window_.Bind(wxEVT_IDLE, &FixtureSymbolPreparationService::OnIdle, this);
  const std::weak_ptr<int> weakLifetime = lifetime_;
  symbols::SetFixtureSymbolPreparationRequestHandler(
      [this, weakLifetime, notifications = notifications_](
          const std::string &path, const std::string &mode) {
        if (wxIsMainThread()) {
          if (weakLifetime.lock())
            EnqueueRequest({path, mode});
        } else {
          notifications->Request({path, mode});
        }
      });
}

// Disconnects fallback requests and invalidates all delayed service callbacks.
FixtureSymbolPreparationService::~FixtureSymbolPreparationService() {
  window_.Unbind(wxEVT_IDLE, &FixtureSymbolPreparationService::OnIdle, this);
  symbols::SetFixtureSymbolPreparationRequestHandler({});
  notifications_->Close();
  lifetime_.reset();
  coordinator_.BeginProjectEpoch();
}

// Invalidates previous work and optionally requests a post-stabilization scan.
void FixtureSymbolPreparationService::BeginProjectEpoch(bool scheduleScan) {
  epoch_ = coordinator_.BeginProjectEpoch();
  work_.clear();
  fixtureHints_.clear();
  manualWork_.clear();
  deferredRequests_.clear();
  fallbackDiagnostics_.clear();
  currentKey_.reset();
  scanFixtureUuids_.clear();
  scanKeys_.clear();
  scanPaths_.clear();
  scanScheduled_ = false;
  stepScheduled_ = false;
  UpdateStatus();
  if (scheduleScan)
    ScheduleScan();
}

// Posts a lifetime-safe scan after the current project operation returns.
void FixtureSymbolPreparationService::ScheduleScan() {
  if (scanScheduled_)
    return;
  scanScheduled_ = true;
  const std::weak_ptr<int> weakLifetime = lifetime_;
  const std::uint64_t requestedEpoch = epoch_;
  window_.CallAfter([this, weakLifetime, requestedEpoch]() {
    if (!weakLifetime.lock() || requestedEpoch != epoch_)
      return;
    scanScheduled_ = false;
    ScanCurrentProject();
  });
}

// Rendering requests already arrive on the GUI thread. Coalesce before posting
// GUI callbacks or doing geometry inspection; non-GUI callers use the mailbox.
void FixtureSymbolPreparationService::EnqueueRequest(
    const symbols::FixtureSymbolPreparationKey &requestedKey) {
  if (requestedKey.effectiveGdtfPath.empty())
    return;
  const symbols::FixtureSymbolPreparationKey key{
      CanonicalResourcePath(requestedKey.effectiveGdtfPath), requestedKey.exactGdtfMode};
  const auto work = work_.find(key);
  const auto *job = coordinator_.Find(key);
  if (work != work_.end() && job && job->epoch == epoch_ &&
      job->state != symbols::FixtureSymbolPreparationState::Cancelled &&
      HasCurrentBinding(key, work->second) &&
      symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath) ==
          work->second.sourceRevision) {
    symbols::diagnostics::Count(symbols::diagnostics::Operation::CoalescedRequest);
    return;
  }
  const bool inserted = deferredRequests_.try_emplace(
      key, symbols::FixtureSymbolPreparationPriority::Automatic).second;
  if (!inserted)
    symbols::diagnostics::Count(symbols::diagnostics::Operation::CoalescedRequest);
  ScheduleNextStep();
}

// Coalesces one physical-resource and exact-mode preparation request.
void FixtureSymbolPreparationService::Request(
    const std::string &effectiveGdtfPath, const std::string &exactGdtfMode,
    symbols::FixtureSymbolPreparationPriority priority) {
  if (effectiveGdtfPath.empty())
    return;
  if (window_.ShouldDeferAutomaticSceneWork()) {
    auto [deferred, inserted] = deferredRequests_.try_emplace(
        symbols::FixtureSymbolPreparationKey{effectiveGdtfPath, exactGdtfMode}, priority);
    if (!inserted && priority == symbols::FixtureSymbolPreparationPriority::Manual)
      deferred->second = priority;
    ScheduleNextStep();
    return;
  }
  const symbols::FixtureSymbolPreparationKey key{
      CanonicalResourcePath(effectiveGdtfPath), exactGdtfMode};
  if (const auto *job = coordinator_.Find(key);
      job && job->epoch == epoch_ &&
      job->state != symbols::FixtureSymbolPreparationState::Cancelled) {
    const auto work = work_.find(key);
    if (work != work_.end() && HasCurrentBinding(key, work->second) &&
        symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath) ==
            work->second.sourceRevision) {
      symbols::diagnostics::Count(symbols::diagnostics::Operation::CoalescedRequest);
      if (priority == symbols::FixtureSymbolPreparationPriority::Manual)
        coordinator_.Request(key, key.effectiveGdtfPath, priority);
      return;
    }
    // A metadata-only change does not invalidate immutable captured geometry.
    // Rehash once in this bounded request slice, never in the rendering bridge.
    if (job->state != symbols::FixtureSymbolPreparationState::Failed &&
        work != work_.end() && HasCurrentBinding(key, work->second) &&
        !work->second.sourceFingerprint.empty()) {
      const auto revision = symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath);
      std::string error;
      const auto fingerprint = symbol_cache::ComputeGdtfSemanticFingerprint(
          key.effectiveGdtfPath, error);
      if (revision.metadataAvailable && fingerprint == work->second.sourceFingerprint &&
          symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath) == revision) {
        work->second.sourceRevision = revision;
        if (priority == symbols::FixtureSymbolPreparationPriority::Manual)
          coordinator_.Request(key, key.effectiveGdtfPath, priority);
        return;
      }
    }
    coordinator_.Cancel(key, epoch_);
    work_.erase(key);
    fallbackDiagnostics_.erase(key);
    if (currentKey_ && *currentKey_ == key)
      currentKey_.reset();
  }
  for (const auto &[fixtureUuid, manualKey] : manualWork_) {
    (void)fixtureUuid;
    if (manualKey == key)
      return;
  }
  const std::string fixtureUuid = FindFixtureUuid(key);
  if (fixtureUuid.empty())
    return;
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto inherited = symbols::InheritFixtureProjectSymbols(
      cfg.GetScene(), cfg.GetProjectFixtureSymbols(), fixtureUuid);
  if (inherited.projectSymbolsUpdated) {
    cfg.MarkDirty();
    window_.RefreshAfterFixtureSymbolUpdate();
  }
  for (const auto &warning : inherited.warnings)
    diagnostics::DiagnosticLogger::Warning(warning);
  const auto &fixture = cfg.GetScene().fixtures.at(fixtureUuid);
  if (!symbols::NeedsAutomaticFixtureSymbolPreparation(
          fixture, cfg.GetProjectFixtureSymbols(), key.effectiveGdtfPath,
          gdtf::MutationPolicy::PreserveImported))
    return;
  if (fallbackDiagnostics_.contains(key))
    return;
  if (!coordinator_.Request(key, key.effectiveGdtfPath, priority))
    return;
  work_.erase(key);
  auto &accepted = work_[key];
  accepted.fixtureUuid = fixtureUuid;
  accepted.sourceSpec = fixture.gdtfSpec;
  accepted.sceneBasePath = cfg.GetScene().basePath;
  accepted.sourceRevision = symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath);
  tools::FixtureGeometryBounds bounds;
  std::string geometryDiagnostic;
  if (!tools::ComputeFixtureGeometryBoundsMm(key.effectiveGdtfPath,
                                            key.exactGdtfMode, bounds,
                                            geometryDiagnostic)) {
    const auto placeholder = symbols::ApplyRuntimeFixtureSymbolPlaceholder(
        cfg.GetScene(), cfg.GetProjectFixtureSymbols(), fixtureUuid,
        key.effectiveGdtfPath);
    if (placeholder.projectSymbolsUpdated) {
      cfg.MarkDirty();
      window_.RefreshAfterFixtureSymbolUpdate();
    }
    if (!placeholder.success)
      diagnostics::DiagnosticLogger::Warning(placeholder.diagnostic);
    if (fallbackDiagnostics_.insert(key).second) {
      diagnostics::DiagnosticLogger::Info(
          "Fixture geometry is unavailable; the current symbol representation is retained. "
          "Project symbol capture was skipped: " + geometryDiagnostic);
    }
    coordinator_.Fail(key, epoch_);
    return;
  }
  {
    work_[key].fixtureUuid = fixtureUuid;
    work_[key].bounds = bounds;
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
    if (symbol_cache::ReadGdtfFileRevision(key.effectiveGdtfPath) !=
        accepted.sourceRevision) {
      coordinator_.Fail(key, epoch_);
      diagnostics::DiagnosticLogger::Warning(
          "Fixture symbol source changed during inspection; retry is required.");
    }
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
  fallbackDiagnostics_.erase(key);
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

// Rechecks PSTG preparation eligibility after manual preview ends.
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
  }
  Request(key.effectiveGdtfPath, key.exactGdtfMode);
}

// Scans unique resolved resources and exact modes after project stabilization.
void FixtureSymbolPreparationService::ScanCurrentProject() {
  scanFixtureUuids_.clear();
  scanKeys_.clear();
  scanPaths_.clear();
  // A fresh explicit scan may discover newly added bindings for an already
  // completed resource. Recheck inheritance without regenerating valid symbols.
  for (auto work = work_.begin(); work != work_.end();) {
    const auto *job = coordinator_.Find(work->first);
    if (job && job->state == symbols::FixtureSymbolPreparationState::Completed &&
        (!currentKey_ || *currentKey_ != work->first)) {
      coordinator_.Cancel(work->first, epoch_);
      work = work_.erase(work);
    } else {
      ++work;
    }
  }
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
  // Retain scheduled work without an idle spin while loading or modal/close
  // confirmation owns the scene. Normal GUI events wake idle after resumption.
  if (!stepScheduled_ || window_.ShouldDeferAutomaticSceneWork())
    return;
  stepScheduled_ = false;
  if (workerPending_ && !workerReady_)
    symbols::diagnostics::Count(symbols::diagnostics::Operation::IdleWhileWorkerBusy);
  if (!deferredRequests_.empty()) {
    const auto request = *deferredRequests_.begin();
    deferredRequests_.erase(deferredRequests_.begin());
    Request(request.first.effectiveGdtfPath, request.first.exactGdtfMode, request.second);
    if (!deferredRequests_.empty()) ScheduleNextStep();
  }
  if (!scanFixtureUuids_.empty()) {
    const std::string fixtureUuid = std::move(scanFixtureUuids_.front());
    scanFixtureUuids_.pop_front();
    const auto &scene =
        GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
    const auto fixtureIt = scene.fixtures.find(fixtureUuid);
    if (fixtureIt != scene.fixtures.end() &&
        !GetDefaultGuiConfigServices().LegacyConfigManager()
             .GetProjectFixtureSymbols().FindForFixture(fixtureIt->second)) {
      fixtures::FixtureGdtfResolution resolution;
      std::string error;
      const auto known = scanPaths_.find(fixtureIt->second.gdtfSpec);
      const bool resolved = known != scanPaths_.end() ||
          fixtures::ResolveFixtureGdtfDeterministic(
              fixtureIt->second, scene, resolution, error, "symbol-scan");
      if (known != scanPaths_.end())
        resolution.selectedPath = known->second;
      if (resolved) {
        scanPaths_.try_emplace(fixtureIt->second.gdtfSpec, resolution.selectedPath);
        const symbols::FixtureSymbolPreparationKey key{
            CanonicalResourcePath(resolution.selectedPath),
            fixtureIt->second.gdtfMode};
        fixtureHints_.try_emplace(key, fixtureUuid);
        if (scanKeys_.insert(key).second)
          Request(key.effectiveGdtfPath, key.exactGdtfMode);
      } else {
        ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
        const auto placeholder = symbols::ApplyRuntimeFixtureSymbolPlaceholder(
            cfg.GetScene(), cfg.GetProjectFixtureSymbols(), fixtureUuid, "");
        if (placeholder.projectSymbolsUpdated) {
          cfg.MarkDirty();
          window_.RefreshAfterFixtureSymbolUpdate();
          diagnostics::DiagnosticLogger::Info(
              "The unavailable fixture definition uses a retained project visual placeholder.");
        }
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
  if (workerPending_) {
    if (!workerReady_)
      return;
    workerReady_ = false;
    workerPending_ = false;
    processingResult_ = processingWorker_.TakeResult();
    if (!currentKey_ || !processingResult_ ||
        processingResult_->epoch != epoch_ ||
        !work_.contains(*currentKey_) ||
        work_.at(*currentKey_).requestId != processingResult_->requestId)
      processingResult_.reset();
  }
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
      if (fixtureIt != scene.fixtures.end() &&
          HasCurrentBinding(*currentKey_, workIt->second)) {
        if (!symbols::NeedsAutomaticFixtureSymbolPreparation(
                fixtureIt->second,
                GetDefaultGuiConfigServices().LegacyConfigManager().GetProjectFixtureSymbols(),
                currentKey_->effectiveGdtfPath,
                gdtf::MutationPolicy::PreserveImported)) {
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
       !HasCurrentBinding(*currentKey_, work))) {
    FailCurrent("Fixture symbol preparation identity became stale.");
    return;
  }

  if (work.stage == WorkStage::Capturing) {
    if (!IsWorkIdentityCurrent(*currentKey_, work)) {
      FailCurrent("Fixture symbol preparation identity became stale before capture.");
      return;
    }
    Viewer2DOffscreenRenderer *renderer = window_.GetOffscreenRenderer();
    if (!renderer) {
      FailCurrent("Fixture symbol preparation could not acquire a renderer.");
      return;
    }
    tools::SceneModelSymbolCaptureOptions options =
        tools::BuildFixtureTypeSymbolCaptureOptions("automatic");
    if (work.bounds.valid)
      options.fixtureBoundsOverride = work.bounds;
    symbols::diagnostics::Count(symbols::diagnostics::Operation::Capture);
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
      work.requestId = ++nextRequestId_;
      if (!processingWorker_.Submit(
              {epoch_, std::move(work.renders), work.bounds, work.requestId})) {
        FailCurrent("Fixture symbol worker rejected an exclusive request.");
        return;
      }
      work.processingSubmitted = true;
      workerPending_ = true;
      return;
    }
    auto processed = std::move(processingResult_);
    processingResult_.reset();
    if (!processed)
      return;
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
    symbols::diagnostics::Count(symbols::diagnostics::Operation::Publication);
    const auto applied = symbols::PrepareFixtureSymbols(
        cfg.GetScene(), cfg.GetProjectFixtureSymbols(), work.fixtureUuid,
        currentKey_->effectiveGdtfPath, work.processedSymbols,
        gdtf::MutationPolicy::PreserveImported);
    if (!coordinator_.IsCurrent(epoch_) || !applied.success) {
      FailCurrent(applied.diagnostic.empty()
                      ? "Fixture symbol application was rejected as stale."
                      : applied.diagnostic);
      return;
    }
    for (const auto &warning : applied.warnings)
      diagnostics::DiagnosticLogger::Warning("Fixture project symbols: " + warning);
    work.projectSymbolsUpdated = applied.projectSymbolsUpdated;
    work.processedSymbols.clear();
    coordinator_.Complete(*currentKey_, epoch_, true);
    work.stage = WorkStage::Finalizing;
    ScheduleNextStep();
    return;
  }

  if (work.projectSymbolsUpdated) {
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
  if (currentKey_) {
    coordinator_.Fail(*currentKey_, epoch_);
    auto work = work_.find(*currentKey_);
    if (work != work_.end()) {
      work->second.renders.clear();
      work->second.processedSymbols.clear();
    }
  }
  diagnostics::DiagnosticLogger::Warning("Fixture symbol preparation failed: " +
                                         diagnostic);
  currentKey_.reset();
  UpdateStatus();
  ScheduleNextStep();
}

// Displays concise aggregate non-modal progress and clears it when idle.
void FixtureSymbolPreparationService::UpdateStatus() {
  if (window_.ShouldDeferAutomaticSceneWork()) return;
  const std::size_t pending = coordinator_.PendingCount();
  if (pending == 0) {
    auto *bar = window_.GetStatusBar();
    if (bar && !lastStatusText_.empty() && bar->GetStatusText(0) == lastStatusText_)
      window_.SetStatusText(_("Ready"), 0);
    lastStatusText_.clear();
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
  wxString context = statusWork ? wxString::FromUTF8(statusWork->displayLabel)
                                : _("fixture resource");
  if (statusKey && !statusKey->exactGdtfMode.empty())
    context += " [" + wxString::FromUTF8(statusKey->exactGdtfMode) + "]";
  wxString phase = _("Preparing");
  if (statusWork) {
    switch (statusWork->stage) {
    case WorkStage::Capturing:
      phase = _("Capturing fixture views");
      break;
    case WorkStage::Processing:
      phase = _("Processing");
      break;
    case WorkStage::Publishing:
      phase = _("Publishing");
      break;
    case WorkStage::Finalizing:
      phase = _("Refreshing");
      break;
    }
  }
  lastStatusText_ = wxString::Format(_("Preparing fixture symbols: %s - %s - %zu pending"),
                                   context, phase, pending);
  window_.SetStatusText(lastStatusText_, 0);
}

// Finds a current-project fixture that owns the exact preparation identity.
std::string FixtureSymbolPreparationService::FindFixtureUuid(
    const symbols::FixtureSymbolPreparationKey &key) {
  const auto &scene =
      GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto hint = fixtureHints_.find(key);
  if (hint != fixtureHints_.end()) {
    const auto fixture = scene.fixtures.find(hint->second);
    fixtures::FixtureGdtfResolution resolution;
    std::string error;
    if (fixture != scene.fixtures.end() &&
        fixture->second.gdtfMode == key.exactGdtfMode &&
        fixtures::ResolveFixtureGdtfDeterministic(
            fixture->second, scene, resolution, error, "symbol-request") &&
        CanonicalResourcePath(resolution.selectedPath) == key.effectiveGdtfPath)
      return hint->second;
    fixtureHints_.erase(hint);
  }
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
  if (!selected.empty())
    fixtureHints_[key] = selected;
  return selected;
}

// Cheap binding checks are safe during immutable processing. Full filesystem
// resolution belongs only at capture/publication boundaries.
bool FixtureSymbolPreparationService::HasCurrentBinding(
    const symbols::FixtureSymbolPreparationKey &key,
    const WorkContext &work) const {
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto fixture = scene.fixtures.find(work.fixtureUuid);
  return scene.basePath == work.sceneBasePath && fixture != scene.fixtures.end() &&
         fixture->second.gdtfSpec == work.sourceSpec &&
         fixture->second.gdtfMode == key.exactGdtfMode;
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
