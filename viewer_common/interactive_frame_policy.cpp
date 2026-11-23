#include "interactive_frame_policy.h"

#include <utility>

namespace interactive_frame {

// Selects the current live pointer when it is valid for the client area.
PointerPosition ResolveLatestPointer(
    PointerPosition queued,
    const std::optional<PointerPosition> &liveInsideClient) {
  return liveInsideClient.value_or(queued);
}

bool PreviewPresentation::TakeInitialPlacementFrame() {
  return std::exchange(m_initialPlacementFrame, false);
}

bool PreviewPresentation::RequestPaint() {
  return !std::exchange(m_paintQueued, true);
}

bool PreviewPresentation::TakePresentedFeedback() {
  return std::exchange(m_feedbackPending, false);
}

void PreviewPresentation::Reset() {
  m_initialPlacementFrame = false;
  m_paintQueued = false;
  m_feedbackPending = false;
}

} // namespace interactive_frame
