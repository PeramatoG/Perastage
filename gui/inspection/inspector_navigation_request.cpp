#include "inspection/inspector_navigation_request.h"

#include "inspector_models.h"

namespace gui::inspection {

// Copies nested-GDTF navigation values out of a model-owned package node.
std::optional<NestedGdtfNavigationRequest>
BuildNestedGdtfNavigationRequest(const PackageTreeNode &node) {
  if (node.resourceKind !=
      perastage::inspection::ResourceKind::NestedGdtf)
    return std::nullopt;
  return NestedGdtfNavigationRequest{node.archivePath, node.sizeKnown,
                                     node.size};
}

} // namespace gui::inspection
