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
  coordinator.PublishSource(sourceA, parent);
  const auto previewA = coordinator.BeginPreview(parent);
  assert(previewA);

  const auto sourceB = coordinator.BeginSourceRequest();
  assert(!coordinator.DisplayedContext());
  assert(!coordinator.BeginPreview(parent));
  assert(!coordinator.AcceptPreview(*previewA));
  coordinator.PublishSource(sourceB, nested);
  assert(coordinator.DisplayedContext() == nested);

  const auto rowPreview = coordinator.BeginPreview(nested);
  assert(rowPreview);
  const auto navigation = coordinator.BeginSourceRequest();
  assert(!coordinator.AcceptPreview(*rowPreview));
  coordinator.PublishSource(navigation, parent);
  assert(coordinator.DisplayedContext()->fingerprint ==
         "parent:same/path.png");
  assert(coordinator.DisplayedContext()->fingerprint != nested->fingerprint);
  return 0;
}
