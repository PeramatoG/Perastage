/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"

namespace scene_parent_transform {
// A root node is local to its layer; grouped nodes are local to their group.
inline Matrix World(const MvrScene &scene, const std::string &parentGroupUuid,
                    const std::string &layerUuid) {
  if (const auto group = scene.groupObjects.find(parentGroupUuid);
      group != scene.groupObjects.end()) return group->second.transform;
  if (const auto layer = scene.layers.find(layerUuid);
      layer != scene.layers.end()) return layer->second.transform;
  return {};
}
template <typename Node>
Matrix World(const MvrScene &scene, const Node &node) {
  return World(scene, node.parentGroupUuid, node.layerUuid);
}
// Supported MVR layers have identity linear placement and vertical elevation.
inline Matrix LayerLocal(const MvrScene &scene, const std::string &layerUuid,
                         Matrix world) {
  world.o[2] -= World(scene, {}, layerUuid).o[2];
  return world;
}
template <typename Node>
void SynchronizeRootLocal(const MvrScene &scene, Node &node) {
  if (!node.parentGroupUuid.empty()) return;
  node.localTransform = LayerLocal(scene, node.layerUuid, node.transform);
  if constexpr (requires { node.hasLocalTransform; }) node.hasLocalTransform = true;
}
} // namespace scene_parent_transform
