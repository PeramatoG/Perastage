#pragma once

#include "navigation_interaction_policy.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace viewer3d::interaction {

enum class HoverTarget { None, Fixtures, Trusses, SceneObjects };

struct HoverQueryInput {
  PointerPosition framebufferPointer;
  HoverTarget target = HoverTarget::None;
  bool hoverExists = false;
  bool skipHeavyLabelWork = false;
  bool navigationActive = false;
  bool selectionDragArmed = false;
  std::chrono::steady_clock::time_point now{};
};

struct HoverQueryDecision {
  bool stateChanged = false;
  bool cadenceDue = false;
  bool paused = false;
  bool shouldRun = false;
  bool shouldClearStaleHover = false;
};

struct InteractionSettleDecision {
  bool pauseHeavyTasks = false;
  bool clearControllerInteraction = false;
  bool requestDeferredResourceSync = false;
  bool markHoverDirty = false;
};

struct ThreadRefreshInput {
  std::size_t cameraFingerprint = 0;
  bool resourceSyncPending = false;
  bool rectangleSelecting = false;
  bool navigationActive = false;
};

struct TelemetrySnapshot {
  int fullRefreshes = 0;
  int highlightRefreshes = 0;
  double averageFullRenderMs = 0.0;
  double averageHoverQueryMs = 0.0;
  double averageHighlightUpdateMs = 0.0;
  int hoverQuerySamples = 0;
  int highlightUpdateSamples = 0;
};

class ViewerRuntimeState {
public:
  using Clock = std::chrono::steady_clock;

  // Records the start or continuation of interactive camera work.
  void BeginInteraction(Clock::time_point now);
  // Clears interactive state after an explicit GUI gesture completion.
  void EndInteraction();
  // Records interaction activity without changing the active flags.
  void TouchInteraction(Clock::time_point now);
  // Evaluates the 200 ms interaction grace period and its settle actions.
  InteractionSettleDecision EvaluateHeavyTaskPause(Clock::time_point now,
                                                   bool fastInteractionMode);

  bool IsInteracting() const { return m_isInteracting; }
  bool IsCameraMoving() const { return m_cameraMoving; }

  // Records camera fingerprint changes and advances the hover revision.
  bool ObserveCameraFingerprint(std::size_t fingerprint);
  // Records hidden-layer fingerprint changes and advances the hover revision.
  bool ObserveHiddenLayersFingerprint(std::size_t fingerprint);
  // Advances the scene revision used to invalidate hover queries.
  void MarkSceneChanged();
  // Advances selection state and schedules a selection refresh.
  void MarkSelectionChanged();
  // Advances highlight state and schedules a highlight refresh.
  void MarkHighlightChanged();

  std::uint64_t CameraRevision() const { return m_cameraRevision; }
  std::uint64_t HiddenLayersRevision() const { return m_hiddenLayersRevision; }
  std::uint64_t SceneRevision() const { return m_sceneRevision; }
  std::uint64_t SelectionRevision() const { return m_selectionRevision; }
  std::uint64_t HighlightRevision() const { return m_highlightRevision; }

  bool SelectionRefreshPending() const { return m_selectionRefreshPending; }
  bool HighlightRefreshPending() const { return m_highlightRefreshPending; }
  // Clears selection refresh only if no newer selection change occurred.
  void CompleteSelectionRefresh(std::uint64_t renderedRevision);
  // Clears highlight refresh only if no newer highlight change occurred.
  void CompleteHighlightRefresh(std::uint64_t renderedRevision);

  // Marks pointer movement as a repaint and forced-hover reason.
  void MarkPointerMoved();
  // Marks pointer state dirty without forcing a cadence-bypassing hover query.
  void MarkPointerDirty();
  // Requests a hover query independently of pointer movement.
  void ForceHoverQuery();
  // Clears the per-frame pointer-movement reason after painting.
  void CompletePointerFrame();
  bool PointerMoved() const { return m_pointerMoved; }
  bool IsHoverQueryForced() const { return m_forceHoverQuery; }

  // Evaluates hover invalidation, cadence, and interaction pause policy.
  HoverQueryDecision EvaluateHoverQuery(const HoverQueryInput &input);
  // Commits a completed pick query and consumes its force request.
  void CompleteHoverQuery(const HoverQueryInput &input);

  // Evaluates whether a worker notification has a visual repaint reason.
  bool ShouldRepaintForThreadRefresh(const ThreadRefreshInput &input);

  // Initializes the 250 ms resource synchronization cadence.
  void InitializeResourceSyncCadence(Clock::time_point now);
  // Reports whether periodic resource synchronization is currently due.
  bool IsResourceSyncCadenceDue(Clock::time_point now) const;
  // Records an accepted resource synchronization cadence opportunity.
  void AcceptResourceSyncCadence(Clock::time_point now);

  // Accumulates one completed full render duration.
  void RecordFullRender(double milliseconds);
  // Accumulates one completed hover query duration.
  void RecordHoverQuery(double milliseconds);
  // Accumulates one completed highlight update duration.
  void RecordHighlightUpdate(double milliseconds);
  // Accumulates a highlight-only refresh count.
  void RecordHighlightRefresh();
  // Returns and resets the one-second telemetry window when it is due.
  std::optional<TelemetrySnapshot> TakeTelemetrySnapshot(Clock::time_point now);

private:
  struct HoverState {
    PointerPosition pointer;
    std::uint64_t cameraRevision = 0;
    std::uint64_t hiddenLayersRevision = 0;
    std::uint64_t sceneRevision = 0;
    HoverTarget target = HoverTarget::None;
  };

  HoverState CurrentHoverState(const HoverQueryInput &input) const;

  Clock::time_point m_lastInteractionTime{};
  bool m_isInteracting = false;
  bool m_cameraMoving = false;
  Clock::time_point m_lastResourceSyncCheck{};

  std::uint64_t m_cameraRevision = 0;
  std::uint64_t m_hiddenLayersRevision = 0;
  std::uint64_t m_sceneRevision = 0;
  std::uint64_t m_selectionRevision = 0;
  std::uint64_t m_highlightRevision = 0;
  std::size_t m_lastCameraFingerprint = 0;
  std::size_t m_lastHiddenLayersFingerprint = 0;
  std::size_t m_lastThreadCameraFingerprint = 0;
  bool m_hasLastThreadCameraFingerprint = false;
  bool m_selectionRefreshPending = false;
  bool m_highlightRefreshPending = false;
  bool m_pointerMoved = false;
  bool m_forceHoverQuery = false;

  HoverState m_lastHoverQueryState{};
  bool m_hasLastHoverQueryState = false;
  HoverTarget m_lastObservedHoverTarget = HoverTarget::None;
  Clock::time_point m_lastHoverQueryTime{};

  Clock::time_point m_telemetryWindowStart{};
  int m_fullRefreshes = 0;
  int m_highlightRefreshes = 0;
  double m_fullRenderMs = 0.0;
  int m_fullRenderSamples = 0;
  double m_hoverQueryMs = 0.0;
  int m_hoverQuerySamples = 0;
  double m_highlightUpdateMs = 0.0;
  int m_highlightUpdateSamples = 0;
};

} // namespace viewer3d::interaction
