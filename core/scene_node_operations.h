#pragma once

#include "scene_grouping.h"

#include <string>
#include <vector>

namespace scene_node_operations {

struct RemovalResult {
  bool changed = false;
  std::vector<scene_grouping::SceneTransformTarget> removedNodes;
  std::vector<std::string> removedEmptyGroups;
  std::vector<std::string> diagnostics;
};

struct ConversionResult {
  bool changed = false;
  std::string uuid;
  std::string error;
};

// Applies an exact world transform while preserving hierarchy-local metadata.
bool ApplyExactWorldTransform(MvrScene &scene, MvrNodeType type,
                              const std::string &uuid,
                              const Matrix &worldTransform);

// Converts one Fixture to a Support atomically while preserving its identity.
ConversionResult ConvertFixtureToSupport(MvrScene &scene,
                                         const std::string &fixtureUuid);

// Removes typed nodes and recursively prunes empty ancestor groups.
RemovalResult RemoveNodes(
    MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets);

} // namespace scene_node_operations
