#include "interactive_frame_policy.h"

#include <cassert>

// Verifies latest-pointer selection and coalesced frame presentation.
int main() {
  const auto latest = interactive_frame::ResolveLatestPointer(
      {105, 100}, interactive_frame::PointerPosition{140, 100});
  assert(latest.x == 140 && latest.y == 100);
  const auto queued =
      interactive_frame::ResolveLatestPointer({105, 100}, std::nullopt);
  assert(queued.x == 105 && queued.y == 100);

  interactive_frame::PreviewPresentation preview;
  assert(preview.RequestPaint());
  preview.MarkTransformApplied();
  assert(!preview.RequestPaint());
  preview.MarkTransformApplied();
  preview.BeginPaint();
  assert(preview.TakePresentedFeedback());
  assert(!preview.TakePresentedFeedback());
  assert(preview.RequestPaint());
  preview.MarkTransformApplied();
  preview.Reset();
  assert(!preview.TakePresentedFeedback());
  assert(preview.RequestPaint());
  return 0;
}
