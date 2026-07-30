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

// Computes the one-shot delta that aligns the raw origin to an absolute
// pointer.
std::array<float, 3>
AbsoluteAlignmentDelta(const std::array<float, 3> &pointerWorld,
                       const std::array<float, 3> &rawOriginWorld) {
  return {pointerWorld[0] - rawOriginWorld[0],
          pointerWorld[1] - rawOriginWorld[1],
          pointerWorld[2] - rawOriginWorld[2]};
}

} // namespace continuous_placement
