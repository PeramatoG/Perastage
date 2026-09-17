#include "viewer2d_runtime_state.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace viewer2d::interaction {

// Marks interaction activity and starts the heavy-work grace period.
void Viewer2DRuntimeState::MarkInteractionActivity(Clock::time_point now) {
  m_isInteracting = true;
  m_lastInteractionTime = now;
}

// Evaluates and settles the heavy-work grace period.
bool Viewer2DRuntimeState::ShouldPauseHeavyTasks(Clock::time_point now) {
  if (!m_isInteracting)
    return false;
  if (now - m_lastInteractionTime <
      std::chrono::milliseconds(kInteractionGracePeriodMs))
    return true;
  m_isInteracting = false;
  return false;
}

// Returns the established hover cadence for current activity.
int Viewer2DRuntimeState::HoverIntervalMilliseconds(bool dragActive) const {
  return dragActive || m_isInteracting || m_viewMotionSinceLastHoverQuery
             ? kInteractingHoverIntervalMs
             : kIdleHoverIntervalMs;
}

// Returns the established pointer threshold for current activity.
int Viewer2DRuntimeState::HoverMoveThresholdPixels(bool dragActive) const {
  return dragActive ? kInteractingMoveThresholdPx : kIdleMoveThresholdPx;
}

// Evaluates whether hover work should run now or after a delay.
HoverScheduleDecision
Viewer2DRuntimeState::ScheduleHover(const HoverScheduleInput &input) {
  m_hoverQueryPending = true;
  const int interval = HoverIntervalMilliseconds(input.dragActive);
  const int threshold = HoverMoveThresholdPixels(input.dragActive);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           input.now - m_lastHoverQueryTime)
                           .count();
  if (!input.forceNow && m_hasLastHoverQueryPointer) {
    const int movement =
        std::abs(input.pointer.x - m_lastHoverQueryPointer.x) +
        std::abs(input.pointer.y - m_lastHoverQueryPointer.y);
    if (movement < threshold && elapsed < interval)
      return {};
  }
  if (input.forceNow || elapsed >= interval)
    return {.runNow = true};
  return {.delayMilliseconds =
              std::max(1, interval - static_cast<int>(elapsed))};
}

// Records a completed hover query and consumes pending view motion.
void Viewer2DRuntimeState::CompleteHoverQuery(PointerPosition pointer,
                                               Clock::time_point now) {
  m_hoverQueryPending = false;
  m_lastHoverQueryPointer = pointer;
  m_hasLastHoverQueryPointer = true;
  m_lastHoverQueryTime = now;
  m_viewMotionSinceLastHoverQuery = false;
}

// Clears pending hover work without completing a query.
void Viewer2DRuntimeState::CancelHoverQuery() { m_hoverQueryPending = false; }

// Marks view motion for interactive cadence and cache invalidation.
void Viewer2DRuntimeState::MarkViewMotion() {
  m_viewMotionSinceLastHoverQuery = true;
  InvalidatePickCache();
}

// Advances the scene generation and invalidates the cached pick.
void Viewer2DRuntimeState::InvalidatePickCache() {
  m_pickCache.valid = false;
  ++m_sceneGeneration;
}

// Invalidates the cache when the hidden-layer fingerprint changes.
void Viewer2DRuntimeState::ObserveHiddenLayersFingerprint(
    std::size_t fingerprint) {
  if (m_hasHiddenLayersFingerprint &&
      m_hiddenLayersFingerprint != fingerprint)
    InvalidatePickCache();
  m_hiddenLayersFingerprint = fingerprint;
  m_hasHiddenLayersFingerprint = true;
}

// Tests whether the cached pick is reusable for the supplied complete key.
bool Viewer2DRuntimeState::IsPickCacheReusable(const PickCacheKey &key) const {
  if (!m_pickCache.valid)
    return false;
  const PickCacheKey &cached = m_pickCache.key;
  if (cached.queryKind != key.queryKind ||
      cached.viewportWidth != key.viewportWidth ||
      cached.viewportHeight != key.viewportHeight || cached.view != key.view ||
      cached.hiddenLayersFingerprint != key.hiddenLayersFingerprint ||
      cached.clickSelection != key.clickSelection ||
      cached.sceneGeneration != key.sceneGeneration)
    return false;
  const int dx = key.framebufferPointer.x - cached.framebufferPointer.x;
  const int dy = key.framebufferPointer.y - cached.framebufferPointer.y;
  return dx * dx + dy * dy <=
         kPickCacheReuseRadiusPx * kPickCacheReuseRadiusPx;
}

// Stores a pick result under the supplied complete key.
void Viewer2DRuntimeState::StorePickCache(PickCacheKey key,
                                           PickCacheResult result) {
  m_pickCache = {.valid = true, .key = key, .result = std::move(result)};
}

} // namespace viewer2d::interaction
