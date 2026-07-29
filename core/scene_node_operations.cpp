#include "scene_node_operations.h"

#include <algorithm>
#include <set>

namespace scene_node_operations {
namespace {

// Removes every matching typed child reference from all groups.
void RemoveChildReferences(MvrScene &scene, MvrNodeType type,
                           const std::string &uuid) {
  for (auto &[groupUuid, group] : scene.groupObjects) {
    std::erase_if(group.children, [&](const GroupObjectChildRef &child) {
      return child.type == type && child.uuid == uuid;
    });
  }
}

// Returns whether a typed scene node currently exists.
bool NodeExists(const MvrScene &scene, MvrNodeType type,
                const std::string &uuid) {
  switch (type) {
  case MvrNodeType::Fixture: return scene.fixtures.contains(uuid);
  case MvrNodeType::Truss: return scene.trusses.contains(uuid);
  case MvrNodeType::Support: return scene.supports.contains(uuid);
  case MvrNodeType::SceneObject: return scene.sceneObjects.contains(uuid);
  case MvrNodeType::GroupObject: return scene.groupObjects.contains(uuid);
  }
  return false;
}

// Removes a node map entry after its hierarchy references have been cleared.
bool EraseNode(MvrScene &scene, MvrNodeType type, const std::string &uuid) {
  RemoveChildReferences(scene, type, uuid);
  switch (type) {
  case MvrNodeType::Fixture:
    for (auto &[supportUuid, support] : scene.supports) {
      if (support.motorFixtureUuid == uuid)
        support.motorFixtureUuid.clear();
    }
    return scene.fixtures.erase(uuid) != 0;
  case MvrNodeType::Truss: return scene.trusses.erase(uuid) != 0;
  case MvrNodeType::Support: return scene.supports.erase(uuid) != 0;
  case MvrNodeType::SceneObject: return scene.sceneObjects.erase(uuid) != 0;
  case MvrNodeType::GroupObject: return scene.groupObjects.erase(uuid) != 0;
  }
  return false;
}

// Recursively collects a GroupObject subtree in child-first order.
void CollectGroupSubtree(const MvrScene &scene, const std::string &groupUuid,
                         std::vector<scene_grouping::SceneTransformTarget> &out,
                         std::set<std::string> &visited) {
  if (!visited.insert(groupUuid).second)
    return;
  const auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return;
  for (const auto &child : groupIt->second.children) {
    if (child.type == MvrNodeType::GroupObject)
      CollectGroupSubtree(scene, child.uuid, out, visited);
    else
      out.push_back({child.type, child.uuid});
  }
  out.push_back({MvrNodeType::GroupObject, groupUuid});
}

// Prunes empty groups until every remaining group owns at least one child.
void PruneEmptyGroups(MvrScene &scene, RemovalResult &result) {
  bool removed = true;
  while (removed) {
    removed = false;
    std::vector<std::string> empty;
    for (const auto &[uuid, group] : scene.groupObjects) {
      if (group.children.empty())
        empty.push_back(uuid);
    }
    std::sort(empty.begin(), empty.end());
    for (const auto &uuid : empty) {
      if (!scene.groupObjects.contains(uuid))
        continue;
      RemoveChildReferences(scene, MvrNodeType::GroupObject, uuid);
      scene.groupObjects.erase(uuid);
      result.removedEmptyGroups.push_back(uuid);
      result.changed = true;
      removed = true;
    }
  }
}

} // namespace

// Applies an exact world transform while preserving hierarchy-local metadata.
bool ApplyExactWorldTransform(MvrScene &scene, MvrNodeType type,
                              const std::string &uuid,
                              const Matrix &worldTransform) {
  if (!NodeExists(scene, type, uuid))
    return false;
  scene_grouping::SetTargetWorldTransform(scene, {type, uuid}, worldTransform);
  return true;
}

// Converts one Fixture to a Support atomically while preserving its identity.
ConversionResult ConvertFixtureToSupport(MvrScene &scene,
                                         const std::string &fixtureUuid) {
  const auto fixtureIt = scene.fixtures.find(fixtureUuid);
  if (fixtureIt == scene.fixtures.end())
    return {.error = "fixture not found"};
  if (scene.supports.contains(fixtureUuid))
    return {.error = "support UUID already exists"};

  const Fixture &fixture = fixtureIt->second;
  Support support;
  support.uuid = fixture.uuid;
  support.name = fixture.instanceName;
  support.gdtfSpec = fixture.gdtfSpec;
  support.gdtfMode = fixture.gdtfMode;
  support.function = fixture.function.empty() ? "Hoist" : fixture.function;
  support.position = fixture.position;
  support.positionName = fixture.positionName;
  support.layer = fixture.layer;
  support.loadKg = fixture.weightKg;
  support.motorName =
      fixture.instanceName.empty() ? fixture.typeName : fixture.instanceName;
  support.motorModel = fixture.gdtfMode;
  support.motorFixtureUuid.clear();
  support.hoistDataSource = "Manual";
  support.hoistFunction = NormalizeHoistFunction(support.function);
  support.transform = fixture.transform;
  support.localTransform = fixture.localTransform;
  support.hasLocalTransform = fixture.hasLocalTransform;
  support.parentGroupUuid = fixture.parentGroupUuid;

  for (auto &[groupUuid, group] : scene.groupObjects) {
    for (auto &child : group.children) {
      if (child.type == MvrNodeType::Fixture && child.uuid == fixtureUuid)
        child.type = MvrNodeType::Support;
    }
  }
  scene.supports.emplace(fixtureUuid, std::move(support));
  scene.fixtures.erase(fixtureIt);
  return {.changed = true, .uuid = fixtureUuid};
}

// Removes typed nodes and recursively prunes empty ancestor groups.
RemovalResult RemoveNodes(
    MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets) {
  RemovalResult result;
  std::vector<scene_grouping::SceneTransformTarget> expanded;
  std::set<std::pair<MvrNodeType, std::string>> requested;
  std::set<std::string> visitedGroups;
  for (const auto &target : targets) {
    if (!requested.insert({target.type, target.uuid}).second)
      continue;
    if (target.type == MvrNodeType::GroupObject)
      CollectGroupSubtree(scene, target.uuid, expanded, visitedGroups);
    else
      expanded.push_back(target);
  }

  std::set<std::pair<MvrNodeType, std::string>> removed;
  for (const auto &target : expanded) {
    if (!removed.insert({target.type, target.uuid}).second)
      continue;
    if (EraseNode(scene, target.type, target.uuid)) {
      result.changed = true;
      result.removedNodes.push_back(target);
    } else {
      result.diagnostics.push_back("node not found: " + target.uuid);
    }
  }
  PruneEmptyGroups(scene, result);
  return result;
}

} // namespace scene_node_operations
