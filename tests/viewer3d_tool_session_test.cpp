#include "../models/continuous_placement_state.h"
#include "../viewer2d/viewer2d_measure_tool.h"
#include "../viewer3d/interaction/line_point_selection_session.h"
#include "../viewer3d/interaction/selection_drag_session.h"

#include <cassert>

// Characterizes the extracted GUI-independent Viewer3D tool sessions.
int main() {
  using viewer3d::interaction::HoverTarget;
  using viewer3d::interaction::SelectionDragSession;

  SelectionDragSession drag;
  assert(drag.Uuids().empty() && !drag.IsUndoPushed() && !drag.PendingSnap());
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {"fixture-a", "fixture-b"};
  selection.trusses = {"truss-a"};
  selection.supports = {"support-a"};
  selection.sceneObjects = {"object-a"};
  drag.Begin(selection, HoverTarget::Fixtures, {1.0f, 2.0f, 3.0f});
  assert((drag.Uuids() == std::vector<std::string>{"fixture-a", "fixture-b",
                                                   "truss-a", "support-a",
                                                   "object-a"}));
  assert(drag.Target() == HoverTarget::Fixtures &&
         drag.AnchorMeters()[1] == 2.0f);
  drag.BeginWithActiveUuids(selection, {"fixture-b"}, HoverTarget::Fixtures,
                            {1.0f, 2.0f, 3.0f});
  assert((drag.Uuids() == std::vector<std::string>{"fixture-b"}));
  assert(drag.Selection().fixtures.size() == 2);
  drag.SetAnchor({4.0f, 5.0f, 6.0f});
  drag.SetAxis(viewer3d::SelectionDragAxis::Y);
  drag.MarkUndoPushed();
  drag.SetPendingSnap(magnet_snap::SnapResult{});
  drag.ClearPendingSnap();
  assert(!drag.PendingSnap());
  drag.SetPendingSnap(magnet_snap::SnapResult{});
  drag.Complete();
  assert(drag.Uuids().empty() && drag.Target() == HoverTarget::None);
  assert(drag.Axis() == viewer3d::SelectionDragAxis::None &&
         !drag.IsUndoPushed());
  assert(!drag.PendingSnap() && drag.AnchorMeters()[0] == 0.0f);
  drag.Begin(selection, HoverTarget::Fixtures, {1.0f, 2.0f, 3.0f});
  drag.Cancel();
  assert(drag.Uuids().empty() && drag.Target() == HoverTarget::None);

  continuous_placement::SessionState placement;
  assert(!placement.IsActive());
  placement.Begin(ContinuousPlacementType::Fixture, "fixture-a");
  assert(placement.IsActive() && placement.Uuid() == "fixture-a");
  placement.RecordConfirmed();
  placement.SetConstraintReference({12, 24}, {1.0f, 2.0f, 3.0f});
  placement.SetAxisSwitchArmed(false);
  placement.MarkViewAligned();
  placement.ContinueWithProvisional("fixture-b");
  assert(placement.ConfirmedUuids().size() == 1 &&
         placement.Uuid() == "fixture-b");
  assert(placement.NeedsAlignment());
  assert(!placement.HasConstraintReference() && placement.IsAxisSwitchArmed());
  placement.SetConstraintReference({12, 24}, {1.0f, 2.0f, 3.0f});
  placement.SetAxisSwitchArmed(false);
  assert(placement.HasConstraintReference() && !placement.IsAxisSwitchArmed());
  placement.ClearConstraintReferencePreservingAxisSwitch();
  assert(!placement.HasConstraintReference() && !placement.IsAxisSwitchArmed());
  placement.RestoreAfterUndo("fixture-a");
  assert(placement.ConfirmedUuids().empty());
  assert(placement.Uuid() == "fixture-a" && placement.IsAxisSwitchArmed());
  placement.RecordConfirmed();
  placement.BeginBatch();
  assert(placement.IsActive() && placement.IsBatchActive());
  assert(placement.Type() == ContinuousPlacementType::None);
  assert(placement.Uuid().empty() && placement.ConfirmedUuids().empty());
  assert(!placement.HasConstraintReference() && placement.IsAxisSwitchArmed());
  placement.Reset();
  assert(!placement.IsActive() && placement.Uuid().empty() &&
         !placement.IsBatchActive());
  assert(!placement.HasConstraintReference() && placement.IsAxisSwitchArmed());
  placement.Begin(ContinuousPlacementType::Fixture, "fixture-c");
  placement.Complete();
  assert(!placement.IsActive() && placement.Uuid().empty());
  placement.Begin(ContinuousPlacementType::Fixture, "fixture-d");
  placement.Cancel();
  assert(!placement.IsActive() && placement.Uuid().empty());

  viewer3d::interaction::LinePointSelectionSession line;
  line.Begin({0.0f, 1.0f, 2.0f}, {3.0f, 4.0f, 5.0f});
  line.MarkMouseUpToConsume();
  line.CommitFirst({1.0f, 2.0f, 3.0f});
  line.SetPreview(std::array<float, 3>{2.0f, 3.0f, 4.0f});
  assert(line.IsActive() && line.First() && line.Preview() &&
         line.HasMouseUpToConsume());
  line.Complete();
  assert(!line.IsActive() && !line.First() && !line.Preview());
  assert(line.ConsumeMouseUp() && !line.ConsumeMouseUp());
  line.Begin({0.0f, 1.0f, 2.0f}, {3.0f, 4.0f, 5.0f});
  line.MarkMouseUpToConsume();
  line.Cancel();
  assert(!line.IsActive() && !line.First() && !line.Preview() &&
         line.HasMouseUpToConsume());
  line.Reset();
  assert(!line.HasMouseUpToConsume());

  Viewer2DMeasureToolState measure;
  measure.enabled = true;
  measure.mode = Viewer2DMeasureMode::EdgeToEdge;
  measure.hasAnchor = true;
  measure.anchorUuid = "fixture-a";
  measure.hasCommittedTarget = true;
  ResetViewer2DMeasure(measure);
  assert(measure.enabled && measure.mode == Viewer2DMeasureMode::EdgeToEdge);
  assert(!measure.hasAnchor && !measure.hasCommittedTarget);
}
