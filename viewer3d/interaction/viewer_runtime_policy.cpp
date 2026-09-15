#include "viewer_runtime_policy.h"

namespace viewer3d::interaction {
namespace {

constexpr auto kInteractionGracePeriod = std::chrono::milliseconds(200);
constexpr auto kHoverQueryInterval = std::chrono::milliseconds(40);
constexpr auto kResourceSyncInterval = std::chrono::milliseconds(250);
constexpr auto kTelemetryWindow = std::chrono::seconds(1);

// Compares two neutral pointer positions.
bool SamePointer(const PointerPosition &left, const PointerPosition &right) {
  return left.x == right.x && left.y == right.y;
}

// Calculates a safe average for a telemetry accumulator.
double Average(double total, int samples) {
  return samples > 0 ? total / static_cast<double>(samples) : 0.0;
}

} // namespace

// Records the start or continuation of interactive camera work.
void ViewerRuntimeState::BeginInteraction(Clock::time_point now) {
  m_isInteracting = true;
  m_cameraMoving = true;
  m_lastInteractionTime = now;
}

// Clears interactive state after an explicit GUI gesture completion.
void ViewerRuntimeState::EndInteraction() {
  m_isInteracting = false;
  m_cameraMoving = false;
}

// Records interaction activity without changing the active flags.
void ViewerRuntimeState::TouchInteraction(Clock::time_point now) {
  m_lastInteractionTime = now;
}

// Evaluates the established interaction grace period and settle actions.
InteractionSettleDecision
ViewerRuntimeState::EvaluateHeavyTaskPause(Clock::time_point now,
                                           bool fastInteractionMode) {
  if ((m_isInteracting || m_cameraMoving) &&
      now - m_lastInteractionTime < kInteractionGracePeriod) {
    return {.pauseHeavyTasks = true};
  }
  if (!m_isInteracting && !m_cameraMoving)
    return {};

  EndInteraction();
  return {.clearControllerInteraction = true,
          .requestDeferredResourceSync = fastInteractionMode,
          .markHoverDirty = fastInteractionMode};
}

// Records camera fingerprint changes and advances the hover revision.
bool ViewerRuntimeState::ObserveCameraFingerprint(std::size_t fingerprint) {
  if (fingerprint == m_lastCameraFingerprint)
    return false;
  m_lastCameraFingerprint = fingerprint;
  ++m_cameraRevision;
  return true;
}

// Records hidden-layer fingerprint changes and advances the hover revision.
bool ViewerRuntimeState::ObserveHiddenLayersFingerprint(
    std::size_t fingerprint) {
  if (fingerprint == m_lastHiddenLayersFingerprint)
    return false;
  m_lastHiddenLayersFingerprint = fingerprint;
  ++m_hiddenLayersRevision;
  return true;
}

// Advances the scene revision used to invalidate hover queries.
void ViewerRuntimeState::MarkSceneChanged() { ++m_sceneRevision; }

// Advances selection state and schedules a selection refresh.
void ViewerRuntimeState::MarkSelectionChanged() {
  ++m_selectionRevision;
  m_selectionRefreshPending = true;
}

// Advances highlight state and schedules a highlight refresh.
void ViewerRuntimeState::MarkHighlightChanged() {
  ++m_highlightRevision;
  m_highlightRefreshPending = true;
}

// Clears selection refresh only if no newer selection change occurred.
void ViewerRuntimeState::CompleteSelectionRefresh(
    std::uint64_t renderedRevision) {
  if (renderedRevision == m_selectionRevision)
    m_selectionRefreshPending = false;
}

// Clears highlight refresh only if no newer highlight change occurred.
void ViewerRuntimeState::CompleteHighlightRefresh(
    std::uint64_t renderedRevision) {
  if (renderedRevision == m_highlightRevision)
    m_highlightRefreshPending = false;
}

// Marks pointer movement as a repaint and forced-hover reason.
void ViewerRuntimeState::MarkPointerMoved() {
  m_pointerMoved = true;
  m_forceHoverQuery = true;
}

// Marks pointer state dirty without forcing a cadence-bypassing hover query.
void ViewerRuntimeState::MarkPointerDirty() { m_pointerMoved = true; }

// Requests a hover query independently of pointer movement.
void ViewerRuntimeState::ForceHoverQuery() { m_forceHoverQuery = true; }

// Clears the per-frame pointer-movement reason after painting.
void ViewerRuntimeState::CompletePointerFrame() { m_pointerMoved = false; }

// Builds the current hover invalidation fingerprint.
ViewerRuntimeState::HoverState
ViewerRuntimeState::CurrentHoverState(const HoverQueryInput &input) const {
  return {input.framebufferPointer, m_cameraRevision, m_hiddenLayersRevision,
          m_sceneRevision, input.target};
}

// Evaluates hover invalidation, cadence, and interaction pause policy.
HoverQueryDecision
ViewerRuntimeState::EvaluateHoverQuery(const HoverQueryInput &input) {
  const HoverState current = CurrentHoverState(input);
  if (input.target != m_lastObservedHoverTarget) {
    m_forceHoverQuery = true;
    m_lastObservedHoverTarget = input.target;
  }
  const bool stateChanged =
      !m_hasLastHoverQueryState ||
      !SamePointer(current.pointer, m_lastHoverQueryState.pointer) ||
      current.cameraRevision != m_lastHoverQueryState.cameraRevision ||
      current.hiddenLayersRevision !=
          m_lastHoverQueryState.hiddenLayersRevision ||
      current.sceneRevision != m_lastHoverQueryState.sceneRevision ||
      current.target != m_lastHoverQueryState.target;
  const bool shouldUpdate =
      m_forceHoverQuery || m_pointerMoved || stateChanged || !input.hoverExists;
  const bool cadenceDue =
      m_forceHoverQuery ||
      input.now - m_lastHoverQueryTime >= kHoverQueryInterval;
  const bool paused = m_cameraMoving || m_isInteracting ||
                      input.navigationActive || input.selectionDragArmed;
  const bool shouldRun =
      !paused && (!input.skipHeavyLabelWork || m_forceHoverQuery) &&
      shouldUpdate && cadenceDue && input.target != HoverTarget::None &&
      (m_forceHoverQuery || stateChanged);
  return {.stateChanged = stateChanged,
          .cadenceDue = cadenceDue,
          .paused = paused,
          .shouldRun = shouldRun,
          .shouldClearStaleHover = input.skipHeavyLabelWork};
}

// Commits a completed pick query and consumes its force request.
void ViewerRuntimeState::CompleteHoverQuery(const HoverQueryInput &input) {
  m_lastHoverQueryState = CurrentHoverState(input);
  m_hasLastHoverQueryState = true;
  m_lastHoverQueryTime = input.now;
  m_forceHoverQuery = false;
}

// Evaluates whether a worker notification has a visual repaint reason.
bool ViewerRuntimeState::ShouldRepaintForThreadRefresh(
    const ThreadRefreshInput &input) {
  const bool cameraChanged =
      !m_hasLastThreadCameraFingerprint ||
      input.cameraFingerprint != m_lastThreadCameraFingerprint;
  if (cameraChanged) {
    m_lastThreadCameraFingerprint = input.cameraFingerprint;
    m_hasLastThreadCameraFingerprint = true;
  }
  return cameraChanged || input.resourceSyncPending ||
         m_selectionRefreshPending || m_highlightRefreshPending ||
         m_pointerMoved || m_forceHoverQuery || input.rectangleSelecting ||
         input.navigationActive || m_isInteracting || m_cameraMoving;
}

// Initializes the established resource synchronization cadence.
void ViewerRuntimeState::InitializeResourceSyncCadence(Clock::time_point now) {
  m_lastResourceSyncCheck = now;
}

// Reports whether periodic resource synchronization is currently due.
bool ViewerRuntimeState::IsResourceSyncCadenceDue(Clock::time_point now) const {
  return now - m_lastResourceSyncCheck >= kResourceSyncInterval;
}

// Records an accepted resource synchronization cadence opportunity.
void ViewerRuntimeState::AcceptResourceSyncCadence(Clock::time_point now) {
  m_lastResourceSyncCheck = now;
}

// Accumulates one completed full render duration.
void ViewerRuntimeState::RecordFullRender(double milliseconds) {
  m_fullRenderMs += milliseconds;
  ++m_fullRenderSamples;
}

// Accumulates one completed hover query duration.
void ViewerRuntimeState::RecordHoverQuery(double milliseconds) {
  m_hoverQueryMs += milliseconds;
  ++m_hoverQuerySamples;
}

// Accumulates one completed highlight update duration.
void ViewerRuntimeState::RecordHighlightUpdate(double milliseconds) {
  m_highlightUpdateMs += milliseconds;
  ++m_highlightUpdateSamples;
}

// Accumulates a highlight-only refresh count.
void ViewerRuntimeState::RecordHighlightRefresh() { ++m_highlightRefreshes; }

// Returns and resets the one-second telemetry window when it is due.
std::optional<TelemetrySnapshot>
ViewerRuntimeState::TakeTelemetrySnapshot(Clock::time_point now) {
  ++m_fullRefreshes;
  if (m_telemetryWindowStart.time_since_epoch().count() == 0) {
    m_telemetryWindowStart = now;
    return std::nullopt;
  }
  if (now - m_telemetryWindowStart < kTelemetryWindow)
    return std::nullopt;

  TelemetrySnapshot snapshot{
      m_fullRefreshes,
      m_highlightRefreshes,
      Average(m_fullRenderMs, m_fullRenderSamples),
      Average(m_hoverQueryMs, m_hoverQuerySamples),
      Average(m_highlightUpdateMs, m_highlightUpdateSamples),
      m_hoverQuerySamples,
      m_highlightUpdateSamples};
  m_telemetryWindowStart = now;
  m_fullRefreshes = 0;
  m_highlightRefreshes = 0;
  m_fullRenderMs = 0.0;
  m_fullRenderSamples = 0;
  m_hoverQueryMs = 0.0;
  m_hoverQuerySamples = 0;
  m_highlightUpdateMs = 0.0;
  m_highlightUpdateSamples = 0;
  return snapshot;
}

} // namespace viewer3d::interaction
