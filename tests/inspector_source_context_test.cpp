#include "inspection/inspector_source_context.h"

#include <cassert>
#include <memory>

namespace {

// Creates a minimal immutable context with a stable cache identity.
std::shared_ptr<const gui::inspection::DisplayedPackageContext>
Context(const char *fingerprint) {
  auto context = std::make_shared<gui::inspection::DisplayedPackageContext>();
  context->fingerprint = fingerprint;
  return context;
}

} // namespace

// Verifies source authority, preview isolation, and atomic parent restoration.
int main() {
  gui::inspection::InspectorRequestCoordinator coordinator;
  const auto parent = Context("parent:same/path.png");
  const auto nested = Context("nested:same/path.png");

  const auto sourceA = coordinator.BeginSourceRequest();
  assert(coordinator.PublishSource(sourceA, parent));
  const auto previewA = coordinator.BeginPreview(parent);
  assert(previewA);

  coordinator.InvalidatePreview();
  assert(!coordinator.AcceptPreview(*previewA));
  const auto replacementPreview = coordinator.BeginPreview(parent);
  assert(replacementPreview);

  const auto sourceB = coordinator.BeginRetainedSourceRequest();
  assert(coordinator.DisplayedContext() == parent);
  const auto pendingPreview = coordinator.BeginPreview(parent);
  assert(pendingPreview);
  coordinator.InvalidatePreview();
  assert(coordinator.SourceGeneration() == sourceB);
  assert(!coordinator.AcceptPreview(*replacementPreview));
  const auto supersedingSource = coordinator.BeginRetainedSourceRequest();
  assert(!coordinator.AcceptPreview(*pendingPreview));
  assert(!coordinator.PublishSource(sourceB, nested));
  assert(coordinator.DisplayedContext() == parent);
  assert(coordinator.PublishSource(supersedingSource, nested));
  assert(coordinator.DisplayedContext() == nested);

  const auto rowPreview = coordinator.BeginPreview(nested);
  assert(rowPreview);
  const auto navigation = coordinator.BeginSourceRequest();
  assert(!coordinator.AcceptPreview(*rowPreview));
  assert(coordinator.PublishSource(navigation, parent));
  assert(coordinator.DisplayedContext()->fingerprint ==
         "parent:same/path.png");
  assert(coordinator.DisplayedContext()->fingerprint != nested->fingerprint);
  return 0;
}
