#pragma once

#include <optional>

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
  void BeginPlacement() { m_initialPlacementFrame = true; }
  bool TakeInitialPlacementFrame();
  void MarkTransformApplied() { m_feedbackPending = true; }
  bool RequestPaint();
  void BeginPaint() { m_paintQueued = false; }
  bool TakePresentedFeedback();
  void Reset();

private:
  bool m_initialPlacementFrame = false;
  bool m_paintQueued = false;
  bool m_feedbackPending = false;
};

} // namespace interactive_frame
