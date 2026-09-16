#include "selection_drag_session.h"

namespace viewer3d::interaction {

// Starts a drag from a typed selection and its world-space anchor.
void SelectionDragSession::Begin(
    const scene_grouping::ObjectSelection &newSelection, HoverTarget newTarget,
    const std::array<float, 3> &newAnchorMeters) {
  Reset();
  selection = newSelection;
  target = newTarget;
  anchorMeters = newAnchorMeters;
  RebuildFlattenedSelection();
}

// Clears all state associated with the current drag.
void SelectionDragSession::Reset() {
  selection = {};
  uuids.clear();
  target = HoverTarget::None;
  anchorMeters = {0.0f, 0.0f, 0.0f};
  axis = SelectionDragAxis::None;
  undoPushed = false;
  pendingSnap.reset();
}

// Rebuilds the stable flattened UUID view from the typed buckets.
void SelectionDragSession::RebuildFlattenedSelection() {
  uuids.clear();
  for (const auto *bucket : {&selection.fixtures, &selection.trusses,
                             &selection.supports, &selection.sceneObjects})
    uuids.insert(uuids.end(), bucket->begin(), bucket->end());
}

// Records a world-space anchor after movement or snap adjustment.
void SelectionDragSession::SetAnchor(
    const std::array<float, 3> &newAnchorMeters) {
  anchorMeters = newAnchorMeters;
}

// Records the currently constrained drag axis.
void SelectionDragSession::SetAxis(SelectionDragAxis newAxis) {
  axis = newAxis;
}

// Records that history has been captured for this gesture.
void SelectionDragSession::MarkUndoPushed() { undoPushed = true; }

} // namespace viewer3d::interaction
