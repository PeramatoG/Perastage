#include "inspection/inspector_navigation_request.h"

#include "inspector_models.h"

#include <cassert>
#include <type_traits>

// Verifies deferred navigation retains values rather than model-owned objects.
int main() {
  using namespace gui::inspection;
  static_assert(!std::is_pointer_v<NestedGdtfNavigationRequest>);
  PackageTreeNode node;
  node.resourceKind = perastage::inspection::ResourceKind::NestedGdtf;
  node.archivePath = "fixtures/example.gdtf";
  node.sizeKnown = true;
  node.size = 42;
  const auto request = BuildNestedGdtfNavigationRequest(node);
  assert(request);
  node.archivePath = "replaced-by-model-reset";
  node.size = 0;
  assert(request->archivePath == "fixtures/example.gdtf");
  assert(request->sizeKnown);
  assert(request->size == 42);

  node.resourceKind = perastage::inspection::ResourceKind::Image;
  assert(!BuildNestedGdtfNavigationRequest(node));
  return 0;
}
