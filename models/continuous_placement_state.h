#pragma once

#include <array>
#include <cstdint>

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
