#include "interactive_frame_policy.h"

#include <utility>

namespace interactive_frame {

// Selects the current live pointer when it is valid for the client area.
PointerPosition ResolveLatestPointer(
    PointerPosition queued,
    const std::optional<PointerPosition> &liveInsideClient) {
  return liveInsideClient.value_or(queued);
}

void PreviewPresentation::BeginPlacement(
    const std::vector<std::string> &fixtureUuids) {
  m_initialPlacementFrame = true;
  m_provisionalFixtures.clear();
  m_provisionalFixtures.insert(fixtureUuids.begin(), fixtureUuids.end());
}

void PreviewPresentation::EndPlacement() {
  m_initialPlacementFrame = false;
  m_provisionalFixtures.clear();
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

void PreviewPresentation::ResetTransform() {
  m_initialPlacementFrame = false;
  m_paintQueued = false;
  m_feedbackPending = false;
}

} // namespace interactive_frame
