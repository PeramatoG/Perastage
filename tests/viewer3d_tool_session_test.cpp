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
  assert(drag.uuids.empty() && !drag.undoPushed && !drag.pendingSnap);
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {"fixture-a", "fixture-b"};
  selection.trusses = {"truss-a"};
  selection.supports = {"support-a"};
  selection.sceneObjects = {"object-a"};
  drag.Begin(selection, HoverTarget::Fixtures, {1.0f, 2.0f, 3.0f});
  assert((drag.uuids == std::vector<std::string>{"fixture-a", "fixture-b",
                                                 "truss-a", "support-a",
                                                 "object-a"}));
  assert(drag.target == HoverTarget::Fixtures && drag.anchorMeters[1] == 2.0f);
  drag.SetAnchor({4.0f, 5.0f, 6.0f});
  drag.SetAxis(viewer3d::SelectionDragAxis::Y);
  drag.MarkUndoPushed();
  drag.pendingSnap = magnet_snap::SnapResult{};
  drag.Reset();
  assert(drag.uuids.empty() && drag.target == HoverTarget::None);
  assert(drag.axis == viewer3d::SelectionDragAxis::None && !drag.undoPushed);
  assert(!drag.pendingSnap && drag.anchorMeters[0] == 0.0f);

  continuous_placement::SessionState placement;
  assert(!placement.active);
  placement.Begin(ContinuousPlacementType::Fixture, "fixture-a");
  assert(placement.active && placement.uuid == "fixture-a");
  placement.RecordConfirmed();
  placement.SetBatchActive(true);
  placement.SetConstraintReference({12, 24}, {1.0f, 2.0f, 3.0f});
  placement.SetAxisSwitchArmed(false);
  assert(placement.confirmedUuids.size() == 1 && placement.batchActive);
  assert(placement.constraintReferenceValid && !placement.axisSwitchArmed);
  placement.RestoreAfterUndo("fixture-a");
  assert(placement.confirmedUuids.empty());
  placement.Reset();
  assert(!placement.active && placement.uuid.empty() && !placement.batchActive);
  assert(!placement.constraintReferenceValid && placement.axisSwitchArmed);

  viewer3d::interaction::LinePointSelectionSession line;
  line.Begin({0.0f, 1.0f, 2.0f}, {3.0f, 4.0f, 5.0f});
  line.consumeMouseUp = true;
  line.CommitFirst({1.0f, 2.0f, 3.0f});
  line.SetPreview(std::array<float, 3>{2.0f, 3.0f, 4.0f});
  assert(line.active && line.first && line.preview && line.consumeMouseUp);
  line.Reset();
  assert(!line.active && !line.first && !line.preview && !line.consumeMouseUp);

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
