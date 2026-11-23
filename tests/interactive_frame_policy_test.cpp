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
  assert(!preview.TakeInitialPlacementFrame());
  preview.BeginPlacement({"preview-a", "preview-b"});
  assert(preview.ProvisionalFixtures().contains("preview-a"));
  assert(preview.ProvisionalFixtures().contains("preview-b"));
  assert(!preview.ProvisionalFixtures().contains("existing-fixture"));
  preview.MarkTransformApplied();
  assert(preview.TakeInitialPlacementFrame());
  assert(!preview.TakeInitialPlacementFrame());
  assert(preview.RequestPaint());
  preview.MarkTransformApplied();
  assert(!preview.RequestPaint());
  preview.MarkTransformApplied();
  preview.BeginPaint();
  assert(preview.TakePresentedFeedback());
  assert(!preview.TakePresentedFeedback());
  assert(preview.RequestPaint());
  preview.MarkTransformApplied();
  preview.ResetTransform();
  assert(preview.ProvisionalFixtures().contains("preview-a"));
  preview.BeginPlacement({"next-preview"});
  assert(!preview.ProvisionalFixtures().contains("preview-a"));
  assert(preview.ProvisionalFixtures().contains("next-preview"));
  preview.EndPlacement();
  preview.ResetTransform();
  assert(preview.ProvisionalFixtures().empty());
  assert(!preview.TakeInitialPlacementFrame());
  assert(!preview.TakePresentedFeedback());
  assert(preview.RequestPaint());
  return 0;
}
