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
  // Starts a drag whose active UUID scope intentionally differs from its typed
  // buckets.
  void BeginWithActiveUuids(const scene_grouping::ObjectSelection &selection,
                            const std::vector<std::string> &activeUuids,
                            HoverTarget target,
                            const std::array<float, 3> &anchorMeters);
  // Clears all state associated with the current drag.
  void Reset();
  // Completes the current drag and clears its transient state.
  void Complete();
  // Cancels the current drag and clears its transient state.
  void Cancel();
  // Rebuilds the stable flattened UUID view from the typed buckets.
  void RebuildFlattenedSelection();
  // Records a world-space anchor after movement or snap adjustment.
  void SetAnchor(const std::array<float, 3> &anchorMeters);
  // Records the currently constrained drag axis.
  void SetAxis(SelectionDragAxis axis);
  // Clears the active drag-axis constraint.
  void ClearAxis();
  // Records that history has been captured for this gesture.
  void MarkUndoPushed();
  // Stores a pending snap preview result.
  void SetPendingSnap(const magnet_snap::SnapResult &snap);
  // Clears the pending snap preview result.
  void ClearPendingSnap();
  // Applies a world-space delta to the current anchor.
  void ApplyAnchorDelta(const std::array<float, 3> &deltaMeters);

  // Returns the typed transform selection for the current drag.
  const scene_grouping::ObjectSelection &Selection() const {
    return selection_;
  }
  // Returns the active UUID scope used by viewer feedback.
  const std::vector<std::string> &Uuids() const { return uuids_; }
  // Returns the table category that initiated the drag.
  HoverTarget Target() const { return target_; }
  // Returns the current world-space drag anchor.
  const std::array<float, 3> &AnchorMeters() const { return anchorMeters_; }
  // Returns the current axis constraint.
  SelectionDragAxis Axis() const { return axis_; }
  // Reports whether undo history has been captured for the drag.
  bool IsUndoPushed() const { return undoPushed_; }
  // Returns the pending snap preview result.
  const std::optional<magnet_snap::SnapResult> &PendingSnap() const {
    return pendingSnap_;
  }

private:
  scene_grouping::ObjectSelection selection_;
  std::vector<std::string> uuids_;
  HoverTarget target_ = HoverTarget::None;
  std::array<float, 3> anchorMeters_{0.0f, 0.0f, 0.0f};
  SelectionDragAxis axis_ = SelectionDragAxis::None;
  bool undoPushed_ = false;
  std::optional<magnet_snap::SnapResult> pendingSnap_;
};

} // namespace viewer3d::interaction
