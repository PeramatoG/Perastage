#include "selection_drag_session.h"

#include <cstddef>

namespace viewer3d::interaction {

// Starts a drag from a typed selection and its world-space anchor.
void SelectionDragSession::Begin(
    const scene_grouping::ObjectSelection &newSelection, HoverTarget newTarget,
    const std::array<float, 3> &newAnchorMeters) {
  Reset();
  selection_ = newSelection;
  target_ = newTarget;
  anchorMeters_ = newAnchorMeters;
  RebuildFlattenedSelection();
}

// Starts a drag with an explicit active UUID scope and typed transform buckets.
void SelectionDragSession::BeginWithActiveUuids(
    const scene_grouping::ObjectSelection &newSelection,
    const std::vector<std::string> &activeUuids, HoverTarget newTarget,
    const std::array<float, 3> &newAnchorMeters) {
  Reset();
  selection_ = newSelection;
  uuids_ = activeUuids;
  target_ = newTarget;
  anchorMeters_ = newAnchorMeters;
}

// Clears all state associated with the current drag.
void SelectionDragSession::Reset() {
  selection_ = {};
  uuids_.clear();
  target_ = HoverTarget::None;
  anchorMeters_ = {0.0f, 0.0f, 0.0f};
  axis_ = SelectionDragAxis::None;
  undoPushed_ = false;
  pendingSnap_.reset();
}

// Completes the current drag and clears its transient state.
void SelectionDragSession::Complete() { Reset(); }

// Cancels the current drag and clears its transient state.
void SelectionDragSession::Cancel() { Reset(); }

// Rebuilds the stable flattened UUID view from the typed buckets.
void SelectionDragSession::RebuildFlattenedSelection() {
  uuids_.clear();
  for (const auto *bucket : {&selection_.fixtures, &selection_.trusses,
                             &selection_.supports, &selection_.sceneObjects})
    uuids_.insert(uuids_.end(), bucket->begin(), bucket->end());
}

// Records a world-space anchor after movement or snap adjustment.
void SelectionDragSession::SetAnchor(
    const std::array<float, 3> &newAnchorMeters) {
  anchorMeters_ = newAnchorMeters;
}

// Records the currently constrained drag axis.
void SelectionDragSession::SetAxis(SelectionDragAxis newAxis) {
  axis_ = newAxis;
}

// Clears the active drag-axis constraint.
void SelectionDragSession::ClearAxis() { axis_ = SelectionDragAxis::None; }

// Records that history has been captured for this gesture.
void SelectionDragSession::MarkUndoPushed() { undoPushed_ = true; }

// Stores a pending snap preview result.
void SelectionDragSession::SetPendingSnap(const magnet_snap::SnapResult &snap) {
  pendingSnap_ = snap;
}

// Clears the pending snap preview result.
void SelectionDragSession::ClearPendingSnap() { pendingSnap_.reset(); }

// Applies a world-space delta to the current anchor.
void SelectionDragSession::ApplyAnchorDelta(
    const std::array<float, 3> &deltaMeters) {
  for (std::size_t axisIndex = 0; axisIndex < anchorMeters_.size(); ++axisIndex)
    anchorMeters_[axisIndex] += deltaMeters[axisIndex];
}

} // namespace viewer3d::interaction
