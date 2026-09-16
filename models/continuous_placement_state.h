#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "continuous_placement_type.h"

namespace continuous_placement {

// Tracks whether a pointer mapping belongs to the current viewport revision.
class ViewRevisionState {
public:
  // Creates stale alignment state at a specified nonzero revision.
  explicit ViewRevisionState(std::uint64_t initialRevision = 1);

  // Invalidates pointer alignment after a camera or viewport transformation.
  void Invalidate();

  // Records that placement was aligned using the current viewport mapping.
  void MarkAligned();

  // Records a completed attempt only when all alignment work succeeded.
  void CompleteAlignmentAttempt(bool succeeded);

  // Reports whether placement must be recomputed from the absolute pointer.
  // Reports whether placement needs realignment to the view.
  bool NeedsAlignment() const;

  // Returns the current revision for deterministic diagnostics and tests.
  std::uint64_t Revision() const { return revision_; }

private:
  std::uint64_t revision_ = 1;
  std::uint64_t alignedRevision_ = 0;
};

// Represents a pointer location without depending on a GUI toolkit.
struct PointerPosition {
  int x = 0;
  int y = 0;
};

// Stores reusable bookkeeping for an active continuous-placement operation.
class SessionState {
public:
  // Starts a placement and clears confirmation history from the prior session.
  void Begin(ContinuousPlacementType type, const std::string &uuid);
  // Starts a clean clipboard batch-placement session.
  void BeginBatch();
  // Replaces the provisional element while preserving confirmation history.
  void ContinueWithProvisional(const std::string &uuid);
  // Records the current provisional element as confirmed.
  void RecordConfirmed();
  // Restores a previously confirmed element as the provisional element.
  void RestoreAfterUndo(const std::string &uuid);
  // Completes the placement sequence and clears its session state.
  void Complete();
  // Cancels the placement sequence and clears its session state.
  void Cancel();
  // Clears the active placement and all transient bookkeeping.
  void Reset();
  // Captures the neutral pointer and world origins used for axis constraints.
  void SetConstraintReference(PointerPosition pointer,
                              const std::array<float, 3> &worldMeters);
  // Clears the active axis constraint reference.
  void ClearConstraintReference();
  // Clears only the active constraint reference and preserves axis-switch
  // state.
  void ClearConstraintReferencePreservingAxisSwitch();
  // Enables or disables the next axis-switch decision.
  void SetAxisSwitchArmed(bool armed);

  // Reports whether placement is active.
  bool IsActive() const { return active_; }
  // Returns the element type owned by the session.
  ContinuousPlacementType Type() const { return type_; }
  // Returns the current provisional element UUID.
  const std::string &Uuid() const { return uuid_; }
  // Returns the confirmed element history.
  const std::vector<std::string> &ConfirmedUuids() const {
    return confirmedUuids_;
  }
  // Reports whether clipboard batch placement is active.
  bool IsBatchActive() const { return batchActive_; }
  // Reports whether an axis constraint reference is available.
  bool HasConstraintReference() const { return constraintReferenceValid_; }
  // Reports whether the next axis-switch decision is armed.
  bool IsAxisSwitchArmed() const { return axisSwitchArmed_; }
  // Returns the neutral pointer origin for constrained placement.
  PointerPosition ConstraintPointerOrigin() const {
    return constraintPointerOrigin_;
  }
  // Returns the world origin for constrained placement.
  const std::array<float, 3> &ConstraintWorldOriginMeters() const {
    return constraintWorldOriginMeters_;
  }
  // Invalidates placement alignment after a view change.
  void InvalidateView() { viewRevision_.Invalidate(); }
  // Marks placement as aligned to the current view.
  void MarkViewAligned() { viewRevision_.MarkAligned(); }
  // Records whether an alignment attempt completed successfully.
  void CompleteAlignmentAttempt(bool succeeded) {
    viewRevision_.CompleteAlignmentAttempt(succeeded);
  }
  bool NeedsAlignment() const { return viewRevision_.NeedsAlignment(); }

private:
  bool active_ = false;
  ContinuousPlacementType type_ = ContinuousPlacementType::None;
  std::string uuid_;
  std::vector<std::string> confirmedUuids_;
  ViewRevisionState viewRevision_;
  bool batchActive_ = false;
  bool constraintReferenceValid_ = false;
  bool axisSwitchArmed_ = true;
  PointerPosition constraintPointerOrigin_;
  std::array<float, 3> constraintWorldOriginMeters_{0.0f, 0.0f, 0.0f};
};

// Computes the one-shot delta that aligns the raw origin to an absolute
// pointer.
std::array<float, 3>
AbsoluteAlignmentDelta(const std::array<float, 3> &pointerWorld,
                       const std::array<float, 3> &rawOriginWorld);

// Removes a temporary preview translation from a displayed world anchor.
std::array<float, 3>
RawAnchorFromPreview(const std::array<float, 3> &displayedAnchorMeters,
                     const std::array<float, 3> &previewDeltaMm);

} // namespace continuous_placement
