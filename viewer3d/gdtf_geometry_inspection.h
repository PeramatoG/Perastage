#pragma once

#include "gdtf_geometry_types.h"
#include <cstddef>

// Produces read-only preview objects in assembled world coordinates. A selected
// Geometry includes its subtree; repeated Model instances keep their node index.
inline std::vector<GdtfObject> GdtfInspectionObjects(
    const GdtfGeometryTree &tree, int selectedNode = -1) {
  std::vector<GdtfObject> objects;
  if (selectedNode < -1 || selectedNode >= static_cast<int>(tree.nodes.size()))
    return objects;
  for (std::size_t index = 0; index < tree.nodes.size(); ++index) {
    bool selected = selectedNode == -1;
    int ancestor = static_cast<int>(index);
    for (std::size_t depth = 0; !selected && ancestor >= 0 &&
         ancestor < static_cast<int>(tree.nodes.size()) && depth < tree.nodes.size(); ++depth) {
      selected = ancestor == selectedNode;
      ancestor = tree.nodes[ancestor].parentIndex;
    }
    const auto &node = tree.nodes[index];
    if (selected && node.hasMesh)
      objects.push_back({node.mesh, node.worldTransform, node.isLens});
  }
  return objects;
}
