#include "scene_clipboard.h"

#include "scene_node_operations.h"
#include "uuidutils.h"

#include <algorithm>

namespace scene_clipboard {
namespace {

// Appends selected values from one typed scene map in selection order.
template <typename T>
void CaptureTyped(const MvrScene &scene, const std::unordered_map<std::string, T> &values,
                  const std::vector<std::string> &selection, MvrNodeType type,
                  const std::unordered_map<std::string, std::string> &overrides,
                  std::vector<Item> &items) {
  for (const auto &uuid : selection) {
    const auto found = values.find(uuid);
    if (found == values.end())
      continue;
    Item item{type, uuid, found->second,
              scene_grouping::GetTargetWorldTransform(scene, {type, uuid})};
    if (type == MvrNodeType::Fixture) {
      const auto overrideIt = overrides.find(uuid);
      if (overrideIt != overrides.end())
        item.fixtureLabelOverride = overrideIt->second;
    }
    items.push_back(std::move(item));
  }
}

// Rebinds a cloned node to an existing source parent or safely roots it.
template <typename T>
void ResolveParent(MvrScene &scene, T &clone, MvrNodeType type,
                   const Matrix &sourceWorld) {
  const auto parent = scene.groupObjects.find(clone.parentGroupUuid);
  if (clone.parentGroupUuid.empty() || parent == scene.groupObjects.end()) {
    clone.parentGroupUuid.clear();
    clone.transform = sourceWorld;
    clone.localTransform = Matrix{};
    clone.hasLocalTransform = false;
    return;
  }
  parent->second.children.push_back({type, clone.uuid});
  clone.transform = sourceWorld;
  scene_grouping::SetTargetWorldTransform(scene, {type, clone.uuid}, sourceWorld);
}

// Adds a value clone after assigning its preallocated identity.
void InsertClone(MvrScene &scene, const Item &item, const std::string &uuid,
                 const std::unordered_map<std::string, std::string> &remap) {
  std::visit([&](const auto &source) {
    using T = std::decay_t<decltype(source)>;
    T clone = source;
    clone.uuid = uuid;
    if constexpr (std::is_same_v<T, Support>) {
      const auto linked = remap.find(clone.motorFixtureUuid);
      if (linked != remap.end())
        clone.motorFixtureUuid = linked->second;
    }
    if constexpr (std::is_same_v<T, Fixture>) {
      scene.fixtures.emplace(uuid, clone);
      ResolveParent(scene, scene.fixtures.at(uuid), MvrNodeType::Fixture,
                    item.sourceWorldTransform);
    } else if constexpr (std::is_same_v<T, Truss>) {
      scene.trusses.emplace(uuid, clone);
      ResolveParent(scene, scene.trusses.at(uuid), MvrNodeType::Truss,
                    item.sourceWorldTransform);
    } else if constexpr (std::is_same_v<T, Support>) {
      scene.supports.emplace(uuid, clone);
      ResolveParent(scene, scene.supports.at(uuid), MvrNodeType::Support,
                    item.sourceWorldTransform);
    } else if constexpr (std::is_same_v<T, SceneObject>) {
      scene.sceneObjects.emplace(uuid, clone);
      ResolveParent(scene, scene.sceneObjects.at(uuid), MvrNodeType::SceneObject,
                    item.sourceWorldTransform);
    }
  }, item.value);
}

} // namespace

// Captures supported selected instances by value without changing scene state.
bool Service::Capture(const MvrScene &scene, const SelectionState &selection,
                      std::uint64_t projectEpoch,
                      const std::unordered_map<std::string, std::string> &overrides) {
  Payload next;
  next.projectEpoch = projectEpoch;
  CaptureTyped(scene, scene.fixtures, selection.GetSelectedFixtures(),
               MvrNodeType::Fixture, overrides, next.items);
  CaptureTyped(scene, scene.trusses, selection.GetSelectedTrusses(),
               MvrNodeType::Truss, overrides, next.items);
  CaptureTyped(scene, scene.supports, selection.GetSelectedSupports(),
               MvrNodeType::Support, overrides, next.items);
  CaptureTyped(scene, scene.sceneObjects, selection.GetSelectedSceneObjects(),
               MvrNodeType::SceneObject, overrides, next.items);
  if (next.Empty())
    return false;
  payload_ = std::move(next);
  return true;
}

// Clones the current payload using a complete two-phase UUID remap.
MutationResult Service::Paste(
    MvrScene &scene, std::uint64_t projectEpoch,
    std::unordered_map<std::string, std::string> *overrides) const {
  MutationResult result;
  if (!CanPaste(projectEpoch))
    return result;
  for (const auto &item : payload_.items)
    result.uuidRemap.emplace(item.sourceUuid, GenerateUuid());
  for (const auto &item : payload_.items) {
    const std::string &uuid = result.uuidRemap.at(item.sourceUuid);
    InsertClone(scene, item, uuid, result.uuidRemap);
    result.nodes.push_back({item.type, uuid});
    if (overrides && item.fixtureLabelOverride)
      (*overrides)[uuid] = *item.fixtureLabelOverride;
  }
  result.changed = !result.nodes.empty();
  return result;
}

// Captures and atomically removes the complete supported mixed selection.
MutationResult Service::Cut(
    MvrScene &scene, SelectionState &selection, std::uint64_t projectEpoch,
    std::unordered_map<std::string, std::string> *overrides) {
  MutationResult result;
  const auto metadata = overrides ? *overrides
                                  : std::unordered_map<std::string, std::string>{};
  if (!Capture(scene, selection, projectEpoch, metadata))
    return result;
  std::vector<scene_grouping::SceneTransformTarget> targets;
  for (const auto &item : payload_.items)
    targets.push_back({item.type, item.sourceUuid});
  const auto removed = scene_node_operations::RemoveNodes(scene, targets);
  if (!removed.changed)
    return result;
  result.changed = true;
  result.nodes = removed.removedNodes;
  if (overrides)
    for (const auto &item : payload_.items)
      overrides->erase(item.sourceUuid);
  selection.Clear();
  return result;
}

// Reports whether payload and current project epoch are compatible.
bool Service::CanPaste(std::uint64_t projectEpoch) const {
  return !payload_.Empty() && payload_.projectEpoch == projectEpoch;
}

// Invalidates all clipboard values.
void Service::Clear() { payload_ = {}; }

// Captures all state needed to roll back provisional placement.
MutationTransaction::MutationTransaction(
    MvrScene &scene, SelectionState &selection, ProjectSession &session,
    std::unordered_map<std::string, std::string> *metadata)
    : scene_(scene), selection_(selection), session_(session), metadata_(metadata),
      sceneBefore_(scene), selectionBefore_(selection),
      dirtyBefore_(session.CaptureDirtyState()),
      metadataBefore_(metadata ? *metadata
                               : std::unordered_map<std::string, std::string>{}) {}

// Rolls back an uncommitted provisional mutation.
MutationTransaction::~MutationTransaction() { Cancel(); }

// Accepts the provisional state and disables rollback.
void MutationTransaction::Commit() { active_ = false; }

// Restores the exact state captured before provisional placement.
void MutationTransaction::Cancel() {
  if (!active_)
    return;
  scene_ = std::move(sceneBefore_);
  selection_ = std::move(selectionBefore_);
  session_.RestoreDirtyState(dirtyBefore_);
  if (metadata_)
    *metadata_ = std::move(metadataBefore_);
  active_ = false;
}

} // namespace scene_clipboard
