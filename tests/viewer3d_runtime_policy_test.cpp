#include "viewer_runtime_policy.h"

#include <cassert>
#include <chrono>

using viewer3d::interaction::HoverQueryInput;
using viewer3d::interaction::HoverTarget;
using viewer3d::interaction::ThreadRefreshInput;
using viewer3d::interaction::ViewerRuntimeState;

// Characterizes GUI-independent Viewer3D runtime orchestration transitions.
int main() {
  using namespace std::chrono_literals;
  using Clock = ViewerRuntimeState::Clock;
  const Clock::time_point start{1ms};

  ViewerRuntimeState interaction;
  assert(!interaction.EvaluateHeavyTaskPause(start, false).pauseHeavyTasks);
  interaction.BeginInteraction(start);
  assert(
      interaction.EvaluateHeavyTaskPause(start + 199ms, false).pauseHeavyTasks);
  const auto settled = interaction.EvaluateHeavyTaskPause(start + 200ms, true);
  assert(!settled.pauseHeavyTasks);
  assert(settled.clearControllerInteraction);
  assert(settled.requestDeferredResourceSync);
  assert(settled.markHoverDirty);
  assert(!interaction.IsInteracting());
  assert(!interaction.IsCameraMoving());

  ViewerRuntimeState hover;
  hover.ObserveCameraFingerprint(10);
  hover.ObserveHiddenLayersFingerprint(20);
  hover.MarkSceneChanged();
  HoverQueryInput query{{10, 20}, HoverTarget::Fixtures, true, false, false,
                        false,    start + 100ms};
  auto decision = hover.EvaluateHoverQuery(query);
  assert(decision.stateChanged && decision.shouldRun);
  hover.CompleteHoverQuery(query);
  decision = hover.EvaluateHoverQuery(query);
  assert(!decision.stateChanged && !decision.shouldRun);
  hover.ForceHoverQuery();
  decision = hover.EvaluateHoverQuery(query);
  assert(!decision.stateChanged && decision.cadenceDue && decision.shouldRun);
  hover.CompleteHoverQuery(query);

  query.framebufferPointer.x++;
  decision = hover.EvaluateHoverQuery(query);
  assert(decision.stateChanged);
  query.framebufferPointer.x--;
  hover.ObserveCameraFingerprint(11);
  assert(hover.EvaluateHoverQuery(query).stateChanged);
  hover.CompleteHoverQuery(query);
  hover.ObserveHiddenLayersFingerprint(21);
  assert(hover.EvaluateHoverQuery(query).stateChanged);
  hover.CompleteHoverQuery(query);
  hover.MarkSceneChanged();
  assert(hover.EvaluateHoverQuery(query).stateChanged);
  hover.CompleteHoverQuery(query);

  query.now += 1ms;
  query.framebufferPointer.x++;
  decision = hover.EvaluateHoverQuery(query);
  assert(!decision.cadenceDue && !decision.shouldRun);
  hover.ForceHoverQuery();
  decision = hover.EvaluateHoverQuery(query);
  assert(decision.cadenceDue && decision.shouldRun);
  hover.CompleteHoverQuery(query);
  assert(!hover.IsHoverQueryForced());

  query.now += 40ms;
  hover.ForceHoverQuery();
  query.navigationActive = true;
  assert(hover.EvaluateHoverQuery(query).paused);
  query.navigationActive = false;
  query.selectionDragArmed = true;
  assert(hover.EvaluateHoverQuery(query).paused);
  query.selectionDragArmed = false;
  hover.BeginInteraction(query.now);
  assert(hover.EvaluateHoverQuery(query).paused);
  hover.EndInteraction();
  query.skipHeavyLabelWork = true;
  assert(hover.EvaluateHoverQuery(query).shouldClearStaleHover);

  ViewerRuntimeState refresh;
  ThreadRefreshInput refreshInput{123, false, false, false};
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  assert(!refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refreshInput.cameraFingerprint = 124;
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refreshInput.cameraFingerprint = 124;
  refreshInput.resourceSyncPending = true;
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refreshInput.resourceSyncPending = false;
  refresh.MarkSelectionChanged();
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refresh.CompleteSelectionRefresh(refresh.SelectionRevision());
  refresh.MarkHighlightChanged();
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refresh.CompleteHighlightRefresh(refresh.HighlightRevision());
  refresh.MarkPointerMoved();
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refresh.CompletePointerFrame();
  refreshInput.rectangleSelecting = true;
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refreshInput.rectangleSelecting = false;
  refreshInput.navigationActive = true;
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refreshInput.navigationActive = false;
  refresh.BeginInteraction(start);
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));
  refresh.EndInteraction();
  refresh.ForceHoverQuery();
  assert(refresh.ShouldRepaintForThreadRefresh(refreshInput));

  ViewerRuntimeState cadence;
  cadence.InitializeResourceSyncCadence(start);
  assert(!cadence.IsResourceSyncCadenceDue(start + 249ms));
  assert(cadence.IsResourceSyncCadenceDue(start + 250ms));
  cadence.AcceptResourceSyncCadence(start + 250ms);
  assert(!cadence.IsResourceSyncCadenceDue(start + 499ms));

  ViewerRuntimeState revisions;
  revisions.MarkSceneChanged();
  revisions.MarkSelectionChanged();
  revisions.MarkHighlightChanged();
  assert(revisions.SceneRevision() == 1);
  assert(revisions.SelectionRevision() == 1);
  assert(revisions.HighlightRevision() == 1);
  const auto oldSelectionRevision = revisions.SelectionRevision();
  revisions.MarkSelectionChanged();
  revisions.CompleteSelectionRefresh(oldSelectionRevision);
  assert(revisions.SelectionRefreshPending());
  revisions.CompleteSelectionRefresh(revisions.SelectionRevision());
  assert(!revisions.SelectionRefreshPending());
  const auto oldHighlightRevision = revisions.HighlightRevision();
  revisions.MarkHighlightChanged();
  revisions.CompleteHighlightRefresh(oldHighlightRevision);
  assert(revisions.HighlightRefreshPending());
  revisions.CompleteHighlightRefresh(revisions.HighlightRevision());
  assert(!revisions.HighlightRefreshPending());

  ViewerRuntimeState telemetry;
  telemetry.RecordFullRender(4.0);
  telemetry.RecordFullRender(6.0);
  telemetry.RecordHoverQuery(8.0);
  telemetry.RecordHighlightUpdate(10.0);
  telemetry.RecordHighlightRefresh();
  assert(!telemetry.TakeTelemetrySnapshot(start));
  const auto snapshot = telemetry.TakeTelemetrySnapshot(start + 1s);
  assert(snapshot);
  assert(snapshot->fullRefreshes == 2);
  assert(snapshot->highlightRefreshes == 1);
  assert(snapshot->averageFullRenderMs == 5.0);
  assert(snapshot->averageHoverQueryMs == 8.0);
  assert(snapshot->averageHighlightUpdateMs == 10.0);
  assert(!telemetry.TakeTelemetrySnapshot(start + 1001ms));
}
