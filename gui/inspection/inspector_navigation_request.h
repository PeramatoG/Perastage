#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace gui::inspection {

struct PackageTreeNode;

// Owns copied values needed after a package activation callback returns.
struct NestedGdtfNavigationRequest final {
  std::string archivePath;
  bool sizeKnown = false;
  std::uint64_t size = 0;
};

// Copies nested-GDTF navigation values out of a model-owned package node.
std::optional<NestedGdtfNavigationRequest>
BuildNestedGdtfNavigationRequest(const PackageTreeNode &node);

} // namespace gui::inspection
