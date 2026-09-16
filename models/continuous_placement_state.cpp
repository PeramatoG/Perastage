#include "continuous_placement_state.h"

#include <limits>

namespace continuous_placement {

// Creates stale alignment state at a specified nonzero revision.
ViewRevisionState::ViewRevisionState(std::uint64_t initialRevision)
    : revision_(initialRevision == 0 ? 1 : initialRevision) {}

// Invalidates pointer alignment after a camera or viewport transformation.
void ViewRevisionState::Invalidate() {
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) {
    revision_ = 1;
    alignedRevision_ = 0;
    return;
  }
  ++revision_;
}

// Records that placement was aligned using the current viewport mapping.
void ViewRevisionState::MarkAligned() { alignedRevision_ = revision_; }

// Records a completed attempt only when all alignment work succeeded.
void ViewRevisionState::CompleteAlignmentAttempt(bool succeeded) {
  if (succeeded)
    MarkAligned();
}

// Reports whether placement must be recomputed from the absolute pointer.
bool ViewRevisionState::NeedsAlignment() const {
  return alignedRevision_ != revision_;
}

// Starts a placement and clears confirmation history from the prior session.
void SessionState::Begin(ContinuousPlacementType newType,
                         const std::string &newUuid) {
  active_ = true;
  type_ = newType;
  uuid_ = newUuid;
  confirmedUuids_.clear();
  viewRevision_.Invalidate();
  ClearConstraintReference();
}

// Starts a clean clipboard batch-placement session.
void SessionState::BeginBatch() {
  Reset();
  active_ = true;
  batchActive_ = true;
  viewRevision_.Invalidate();
}

// Replaces the provisional element while preserving confirmation history.
void SessionState::ContinueWithProvisional(const std::string &newUuid) {
  uuid_ = newUuid;
  viewRevision_.Invalidate();
  ClearConstraintReference();
}

// Records the current provisional element as confirmed.
void SessionState::RecordConfirmed() { confirmedUuids_.push_back(uuid_); }

// Restores a previously confirmed element as the provisional element.
void SessionState::RestoreAfterUndo(const std::string &restoredUuid) {
  if (!confirmedUuids_.empty() && confirmedUuids_.back() == restoredUuid)
    confirmedUuids_.pop_back();
  ContinueWithProvisional(restoredUuid);
}

// Completes the placement sequence and clears its session state.
void SessionState::Complete() { Reset(); }

// Cancels the placement sequence and clears its session state.
void SessionState::Cancel() { Reset(); }

// Clears the active placement and all transient bookkeeping.
void SessionState::Reset() {
  active_ = false;
  type_ = ContinuousPlacementType::None;
  uuid_.clear();
  confirmedUuids_.clear();
  batchActive_ = false;
  ClearConstraintReference();
}

// Captures the neutral pointer and world origins used for axis constraints.
void SessionState::SetConstraintReference(
    PointerPosition pointer, const std::array<float, 3> &worldMeters) {
  constraintPointerOrigin_ = pointer;
  constraintWorldOriginMeters_ = worldMeters;
  constraintReferenceValid_ = true;
}

// Clears the active axis constraint reference.
void SessionState::ClearConstraintReference() {
  ClearConstraintReferencePreservingAxisSwitch();
  axisSwitchArmed_ = true;
}

// Clears only the active constraint reference and preserves axis-switch state.
void SessionState::ClearConstraintReferencePreservingAxisSwitch() {
  constraintReferenceValid_ = false;
  constraintPointerOrigin_ = {};
  constraintWorldOriginMeters_ = {0.0f, 0.0f, 0.0f};
}

// Enables or disables the next axis-switch decision.
void SessionState::SetAxisSwitchArmed(bool armed) { axisSwitchArmed_ = armed; }

// Computes the one-shot delta that aligns the raw origin to an absolute
// pointer.
std::array<float, 3>
AbsoluteAlignmentDelta(const std::array<float, 3> &pointerWorld,
                       const std::array<float, 3> &rawOriginWorld) {
  return {pointerWorld[0] - rawOriginWorld[0],
          pointerWorld[1] - rawOriginWorld[1],
          pointerWorld[2] - rawOriginWorld[2]};
}

// Removes a temporary preview translation from a displayed world anchor.
std::array<float, 3>
RawAnchorFromPreview(const std::array<float, 3> &displayedAnchorMeters,
                     const std::array<float, 3> &previewDeltaMm) {
  return {displayedAnchorMeters[0] - previewDeltaMm[0] / 1000.0f,
          displayedAnchorMeters[1] - previewDeltaMm[1] / 1000.0f,
          displayedAnchorMeters[2] - previewDeltaMm[2] / 1000.0f};
}

} // namespace continuous_placement
