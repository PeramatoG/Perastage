#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace interactive_frame {

struct PointerPosition {
  int x = 0;
  int y = 0;
};

PointerPosition ResolveLatestPointer(
    PointerPosition queued,
    const std::optional<PointerPosition> &liveInsideClient);

// Coalesces viewport repaint requests and live feedback independently.
class PreviewPresentation {
public:
  void BeginPlacement(const std::vector<std::string> &fixtureUuids);
  void EndPlacement();
  const std::unordered_set<std::string> &ProvisionalFixtures() const {
    return m_provisionalFixtures;
  }
  bool TakeInitialPlacementFrame();
  void MarkTransformApplied() { m_feedbackPending = true; }
  bool RequestPaint();
  void BeginPaint() { m_paintQueued = false; }
  bool TakePresentedFeedback();
  void ResetTransform();

private:
  std::unordered_set<std::string> m_provisionalFixtures;
  bool m_initialPlacementFrame = false;
  bool m_paintQueued = false;
  bool m_feedbackPending = false;
};

} // namespace interactive_frame
