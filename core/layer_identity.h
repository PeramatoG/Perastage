#pragma once

#include "mvrscene.h"
#include "scene_parent_transform.h"
#include <unordered_set>

namespace layerdomain {

std::string DefaultLayerUuid();
const Layer *FindLayer(const MvrScene &scene, const std::string &uuid);
std::string PresentationName(const MvrScene &scene, const std::string &uuid);
// UUID first, then an unambiguous presentation label or legacy authored name.
std::string ResolveLayerReference(const MvrScene &scene,
                                  const std::string &reference);
Layer &EnsureDefaultLayer(MvrScene &scene);
std::unordered_set<std::string> ResolveLayerStateReferences(
    const MvrScene &scene, const std::unordered_set<std::string> &references);
std::string EnsureLayerReference(MvrScene &scene, const std::string &reference);
// Migration is explicit: never invoke it from a read-only scene accessor.
void MigrateLayerMembership(MvrScene &scene);
void RefreshLayerLabels(MvrScene &scene);

// Table adapters prepare values without creating layers before an Undo snapshot.
template <typename Object>
void PrepareLayerEdit(const MvrScene &scene, Object &object,
                      const std::string &reference) {
  if (!object.layerUuid.empty() &&
      (reference == object.layer ||
       (reference.empty() && object.layerUuid == DefaultLayerUuid())))
    return;
  const auto previousLayer = object.layerUuid;
  object.layerUuid = ResolveLayerReference(scene, reference);
  object.layer = object.layerUuid.empty()
                     ? reference : PresentationName(scene, object.layerUuid);
  if (object.layerUuid != previousLayer)
    scene_parent_transform::SynchronizeRootLocal(scene, object);
}

template <typename Object>
void AssignLayer(MvrScene &scene, Object &object, const std::string &reference) {
  const auto previousLayer = object.layerUuid;
  object.layerUuid = EnsureLayerReference(scene, reference);
  object.layer = PresentationName(scene, object.layerUuid);
  if (object.layerUuid != previousLayer)
    scene_parent_transform::SynchronizeRootLocal(scene, object);
}

template <typename Object>
void CommitLayerEdit(MvrScene &scene, Object &object) {
  if (object.layerUuid.empty()) {
    AssignLayer(scene, object, object.layer);
    RefreshLayerLabels(scene);
  }
}

} // namespace layerdomain
