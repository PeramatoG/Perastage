#pragma once

#include "../../core/magnet_snap.h"
#include "../../core/scene_grouping.h"
#include "selection_drag_math.h"
#include "viewer_runtime_policy.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace viewer3d::interaction {

// Stores neutral state for one selection manipulation gesture.
class SelectionDragSession {
public:
  // Starts a drag from a typed selection and its world-space anchor.
  void Begin(const scene_grouping::ObjectSelection &selection,
             HoverTarget target, const std::array<float, 3> &anchorMeters);
  // Clears all state associated with the current drag.
  void Reset();
  // Rebuilds the stable flattened UUID view from the typed buckets.
  void RebuildFlattenedSelection();
  // Records a world-space anchor after movement or snap adjustment.
  void SetAnchor(const std::array<float, 3> &anchorMeters);
  // Records the currently constrained drag axis.
  void SetAxis(SelectionDragAxis axis);
  // Records that history has been captured for this gesture.
  void MarkUndoPushed();

  scene_grouping::ObjectSelection selection;
  std::vector<std::string> uuids;
  HoverTarget target = HoverTarget::None;
  std::array<float, 3> anchorMeters{0.0f, 0.0f, 0.0f};
  SelectionDragAxis axis = SelectionDragAxis::None;
  bool undoPushed = false;
  std::optional<magnet_snap::SnapResult> pendingSnap;
};

} // namespace viewer3d::interaction
