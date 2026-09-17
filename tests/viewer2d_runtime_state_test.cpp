#include "viewer2d_runtime_state.h"

#include <cassert>
#include <chrono>

namespace {

using viewer2d::interaction::HoverScheduleInput;
using viewer2d::interaction::PickCacheKey;
using viewer2d::interaction::PickQueryKind;
using viewer2d::interaction::Viewer2DRuntimeState;
using Clock = Viewer2DRuntimeState::Clock;

// Creates a deterministic time point for runtime-policy checks.
Clock::time_point At(int milliseconds) {
  return Clock::time_point(std::chrono::milliseconds(milliseconds));
}

// Creates the complete baseline cache key used by reuse checks.
PickCacheKey CacheKey() {
  return {PickQueryKind::FixtureLabel, {20, 30}, 800, 600, 2, 41, false, 0};
}

// Verifies interaction settling, cadence, thresholds, and scheduling.
void TestHoverAndInteractionPolicy() {
  Viewer2DRuntimeState state;
  assert(state.HoverIntervalMilliseconds(false) == 10);
  assert(state.HoverMoveThresholdPixels(false) == 0);

  state.MarkInteractionActivity(At(100));
  assert(state.IsInteracting());
  assert(state.ShouldPauseHeavyTasks(At(299)));
  assert(!state.ShouldPauseHeavyTasks(At(300)));
  assert(!state.IsInteracting());

  state.MarkInteractionActivity(At(400));
  assert(state.HoverIntervalMilliseconds(false) == 35);
  assert(state.HoverMoveThresholdPixels(true) == 3);
  state.CompleteHoverQuery({10, 10}, At(400));

  auto decision = state.ScheduleHover({{11, 10}, false, true, At(410)});
  assert(!decision.runNow && decision.delayMilliseconds == 0);
  decision = state.ScheduleHover({{15, 10}, false, true, At(410)});
  assert(!decision.runNow && decision.delayMilliseconds == 25);
  decision = state.ScheduleHover({{15, 10}, true, true, At(411)});
  assert(decision.runNow);
  state.CompleteHoverQuery({15, 10}, At(411));
  decision = state.ScheduleHover({{15, 10}, false, true, At(446)});
  assert(decision.runNow);

  assert(!state.ShouldPauseHeavyTasks(At(600)));
  state.MarkViewMotion();
  assert(state.HoverIntervalMilliseconds(false) == 35);
  state.CompleteHoverQuery({15, 10}, At(600));
  assert(state.HoverIntervalMilliseconds(false) == 10);
  state.CancelHoverQuery();
  assert(!state.IsHoverQueryPending());
}

// Verifies every behaviorally relevant pick-cache key dimension.
void TestPickCachePolicy() {
  Viewer2DRuntimeState state;
  PickCacheKey key = CacheKey();
  key.sceneGeneration = state.SceneGeneration();
  state.StorePickCache(key, {true, "fixture-1"});
  assert(state.IsPickCacheReusable(key));
  assert(state.CachedPickResult().found);
  assert(state.CachedPickResult().uuid == "fixture-1");

  PickCacheKey changed = key;
  changed.framebufferPointer = {23, 30};
  assert(state.IsPickCacheReusable(changed));
  changed.framebufferPointer = {24, 30};
  assert(!state.IsPickCacheReusable(changed));

  changed = key;
  changed.queryKind = PickQueryKind::TrussLabel;
  assert(!state.IsPickCacheReusable(changed));
  changed = key;
  ++changed.viewportWidth;
  assert(!state.IsPickCacheReusable(changed));
  changed = key;
  ++changed.viewportHeight;
  assert(!state.IsPickCacheReusable(changed));
  changed = key;
  ++changed.view;
  assert(!state.IsPickCacheReusable(changed));
  changed = key;
  changed.clickSelection = true;
  assert(!state.IsPickCacheReusable(changed));
  changed = key;
  ++changed.sceneGeneration;
  assert(!state.IsPickCacheReusable(changed));
  changed = key;
  ++changed.hiddenLayersFingerprint;
  assert(!state.IsPickCacheReusable(changed));

  state.InvalidatePickCache();
  assert(!state.IsPickCacheReusable(key));
  key.sceneGeneration = state.SceneGeneration();
  state.StorePickCache(key, {false, {}});
  state.ObserveHiddenLayersFingerprint(key.hiddenLayersFingerprint);
  assert(state.IsPickCacheReusable(key));
  state.ObserveHiddenLayersFingerprint(key.hiddenLayersFingerprint + 1);
  assert(!state.IsPickCacheReusable(key));

  key.sceneGeneration = state.SceneGeneration();
  state.StorePickCache(key, {true, "fixture-2"});
  state.MarkViewMotion();
  assert(!state.IsPickCacheReusable(key));
}

} // namespace

// Runs GUI-independent Viewer2D runtime characterization checks.
int main() {
  TestHoverAndInteractionPolicy();
  TestPickCachePolicy();
  return 0;
}
