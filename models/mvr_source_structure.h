#pragma once
#include "sceneobject.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

// Import provenance for standard topology and unedited geometry. Editable
// values remain in the normal scene models. Canonical XML reconstructs this on
// reopen.
struct MvrSourceNode {
  std::string uuid, kind, parentUuid, layerUuid, projectedParentUuid;
  std::string xml; // Validated shallow node, with an empty authored ChildList.
  Matrix authoredWorld, displayedWorld;
  std::optional<MvrMatrixSource> localSource;
  std::vector<GeometryInstance> projectedGeometries;
  std::string projectedModel;
  bool modeled = false, valid = false, geometryAuthority = false,
       editedGeometry = false;
};
struct MvrSourceStructure {
  unsigned version = 1;
  std::vector<MvrSourceNode> nodes;
  std::unordered_map<std::string, size_t> nodeIndex, symbolOwnerIndex;
};
