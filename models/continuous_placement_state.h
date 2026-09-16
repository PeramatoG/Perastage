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
  // Records the current provisional element as confirmed.
  void RecordConfirmed();
  // Restores a previously confirmed element as the provisional element.
  void RestoreAfterUndo(const std::string &uuid);
  // Clears the active placement and all transient bookkeeping.
  void Reset();
  // Enables or disables batch placement.
  void SetBatchActive(bool active);
  // Captures the neutral pointer and world origins used for axis constraints.
  void SetConstraintReference(PointerPosition pointer,
                              const std::array<float, 3> &worldMeters);
  // Clears the active axis constraint reference.
  void ClearConstraintReference();
  // Enables or disables the next axis-switch decision.
  void SetAxisSwitchArmed(bool armed);

  bool active = false;
  ContinuousPlacementType type = ContinuousPlacementType::None;
  std::string uuid;
  std::vector<std::string> confirmedUuids;
  ViewRevisionState viewRevision;
  bool batchActive = false;
  bool constraintReferenceValid = false;
  bool axisSwitchArmed = true;
  PointerPosition constraintPointerOrigin;
  std::array<float, 3> constraintWorldOriginMeters{0.0f, 0.0f, 0.0f};
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
