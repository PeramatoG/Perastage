#pragma once

#include "mvrscene.h"

namespace scene_grouping {

// Defines which selected leaf node types promote to their highest parent group.
struct InteractiveTransformPolicy {
  bool promoteFixturesToGroup = false;
  bool promoteTrussesToGroup = true;
  bool promoteSupportsToGroup = false;
  bool promoteSceneObjectsToGroup = false;

  // Returns whether interactive transforms promote the supplied leaf type.
  bool PromotesToGroup(MvrNodeType type) const {
    switch (type) {
    case MvrNodeType::Fixture:
      return promoteFixturesToGroup;
    case MvrNodeType::Truss:
      return promoteTrussesToGroup;
    case MvrNodeType::Support:
      return promoteSupportsToGroup;
    case MvrNodeType::SceneObject:
      return promoteSceneObjectsToGroup;
    case MvrNodeType::GroupObject:
      return true;
    }
    return false;
  }
};

} // namespace scene_grouping
