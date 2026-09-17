/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License v3.0
 */

#pragma once

#include "viewer2d_interaction_session.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace viewer2d::interaction {

enum class PickQueryKind {
  None,
  FixtureLabel,
  TrussLabel,
  HoistLabel,
  SceneObjectLabel,
  PickUuid
};

struct HoverScheduleInput {
  PointerPosition pointer;
  bool forceNow = false;
  bool dragActive = false;
  std::chrono::steady_clock::time_point now{};
};

struct HoverScheduleDecision {
  bool runNow = false;
  int delayMilliseconds = 0;
};

struct PickCacheKey {
  PickQueryKind queryKind = PickQueryKind::None;
  PointerPosition framebufferPointer;
  int viewportWidth = 0;
  int viewportHeight = 0;
  int view = 0;
  std::size_t hiddenLayersFingerprint = 0;
  bool clickSelection = false;
  std::uint64_t sceneGeneration = 0;
};

struct PickCacheResult {
  bool found = false;
  std::string uuid;
};

// Owns GUI-independent Viewer2D hover scheduling and pick-cache state.
class Viewer2DRuntimeState {
public:
  using Clock = std::chrono::steady_clock;

  // Marks interaction activity and starts the heavy-work grace period.
  void MarkInteractionActivity(Clock::time_point now);
  // Evaluates and settles the heavy-work grace period.
  bool ShouldPauseHeavyTasks(Clock::time_point now);
  // Reports whether the runtime is inside the interaction grace period.
  bool IsInteracting() const { return m_isInteracting; }

  // Returns the established hover cadence for current activity.
  int HoverIntervalMilliseconds(bool dragActive) const;
  // Returns the established pointer threshold for current activity.
  int HoverMoveThresholdPixels(bool dragActive) const;
  // Evaluates whether hover work should run now or after a delay.
  HoverScheduleDecision ScheduleHover(const HoverScheduleInput &input);
  // Records a completed hover query and consumes pending view motion.
  void CompleteHoverQuery(PointerPosition pointer, Clock::time_point now);
  // Clears pending hover work without completing a query.
  void CancelHoverQuery();
  // Reports whether hover work remains pending.
  bool IsHoverQueryPending() const { return m_hoverQueryPending; }
  // Marks view motion for interactive cadence and cache invalidation.
  void MarkViewMotion();

  // Advances the scene generation and invalidates the cached pick.
  void InvalidatePickCache();
  // Invalidates the cache when the hidden-layer fingerprint changes.
  void ObserveHiddenLayersFingerprint(std::size_t fingerprint);
  // Tests whether the cached pick is reusable for the supplied complete key.
  bool IsPickCacheReusable(const PickCacheKey &key) const;
  // Stores a pick result under the supplied complete key.
  void StorePickCache(PickCacheKey key, PickCacheResult result);
  // Returns the result associated with the reusable cache entry.
  const PickCacheResult &CachedPickResult() const { return m_pickCache.result; }
  // Returns the current explicit scene invalidation generation.
  std::uint64_t SceneGeneration() const { return m_sceneGeneration; }

  static constexpr int kIdleHoverIntervalMs = 10;
  static constexpr int kInteractingHoverIntervalMs = 35;
  static constexpr int kInteractingMoveThresholdPx = 3;
  static constexpr int kIdleMoveThresholdPx = 0;
  static constexpr int kInteractionGracePeriodMs = 200;
  static constexpr int kPickCacheReuseRadiusPx = 3;

private:
  struct PickCacheEntry {
    bool valid = false;
    PickCacheKey key;
    PickCacheResult result;
  };

  Clock::time_point m_lastInteractionTime{};
  Clock::time_point m_lastHoverQueryTime{};
  PointerPosition m_lastHoverQueryPointer;
  bool m_isInteracting = false;
  bool m_hoverQueryPending = false;
  bool m_hasLastHoverQueryPointer = false;
  bool m_viewMotionSinceLastHoverQuery = false;
  std::uint64_t m_sceneGeneration = 0;
  std::size_t m_hiddenLayersFingerprint = 0;
  bool m_hasHiddenLayersFingerprint = false;
  PickCacheEntry m_pickCache;
};

} // namespace viewer2d::interaction
