#include "layer_identity.h"

#include "uuidutils.h"

#include <algorithm>
#include <map>

namespace layerdomain {
namespace {
constexpr const char *kDefaultName = "No Layer";

template <typename Action> void VisitObjects(MvrScene &scene, Action action) {
  for (auto &[uuid, object] : scene.fixtures) action(uuid, object);
  for (auto &[uuid, object] : scene.trusses) action(uuid, object);
  for (auto &[uuid, object] : scene.supports) action(uuid, object);
  for (auto &[uuid, object] : scene.sceneObjects) action(uuid, object);
  for (auto &[uuid, object] : scene.groupObjects) action(uuid, object);
}
} // namespace

std::string DefaultLayerUuid() {
  return DeriveDeterministicUuid("mvr:layer:default");
}

const Layer *FindLayer(const MvrScene &scene, const std::string &uuid) {
  const auto it = scene.layers.find(uuid);
  return it == scene.layers.end() ? nullptr : &it->second;
}

std::string PresentationName(const MvrScene &scene, const std::string &uuid) {
  const auto *layer = FindLayer(scene, uuid);
  if (!layer) return {};
  bool disambiguate = layer->name.empty() || layer->name == "layer_default" ||
      (scene.layers.contains(layer->name) && layer->name != uuid);
  for (const auto &[otherUuid, other] : scene.layers)
    if (otherUuid != uuid && other.name == layer->name) disambiguate = true;
  std::string label = layer->name;
  if (disambiguate) label += (label.empty() ? "[" : " [") + uuid + "]";
  // Authored names may themselves look like generated labels.
  while (std::any_of(scene.layers.begin(), scene.layers.end(),
                    [&](const auto &entry) {
                      return entry.first != uuid && entry.second.name == label;
                    }))
    label += " [" + uuid + "]";
  return label;
}

std::string ResolveLayerReference(const MvrScene &scene,
                                  const std::string &reference) {
  if (reference.empty() || reference == "layer_default")
    return scene.layers.contains(DefaultLayerUuid()) ? DefaultLayerUuid() : "";
  const auto canonical = CanonicalizeUuid(reference);
  if (!canonical.empty() && scene.layers.contains(canonical)) return canonical;
  std::string found;
  for (const auto &[uuid, layer] : scene.layers) {
    if (PresentationName(scene, uuid) == reference || layer.name == reference) {
      if (!found.empty()) return {}; // Never guess between equal authored names.
      found = uuid;
    }
  }
  return found;
}

std::unordered_set<std::string> ResolveLayerStateReferences(
    const MvrScene &scene, const std::unordered_set<std::string> &references) {
  std::unordered_set<std::string> result;
  for (const auto &reference : references) {
    const auto uuid = ResolveLayerReference(scene, reference);
    if (!uuid.empty()) { result.insert(uuid); continue; }
    bool found = false;
    // Historical visibility hid every layer sharing this authored name.
    for (const auto &[key, layer] : scene.layers)
      if (layer.name == reference) { result.insert(key); found = true; }
    if (!found) result.insert(reference); // Keep state for currently absent layers.
  }
  return result;
}

Layer &EnsureDefaultLayer(MvrScene &scene) {
  const auto uuid = DefaultLayerUuid();
  return scene.layers.try_emplace(uuid, Layer{uuid, kDefaultName, {}, {}})
      .first->second;
}

std::string EnsureLayerReference(MvrScene &scene, const std::string &reference) {
  if (const auto uuid = ResolveLayerReference(scene, reference); !uuid.empty())
    return uuid;
  if (reference.empty() || reference == "layer_default" || reference == kDefaultName)
    return EnsureDefaultLayer(scene).uuid;
  const auto uuid = CanonicalizeUuid(GenerateUuid());
  scene.layers.emplace(uuid, Layer{uuid, reference, {}, {}});
  return uuid;
}

void RefreshLayerLabels(MvrScene &scene) {
  VisitObjects(scene, [&](const auto &, auto &object) {
    if (FindLayer(scene, object.layerUuid))
      object.layer = PresentationName(scene, object.layerUuid);
  });
}

void MigrateLayerMembership(MvrScene &scene) {
  std::string legacyOwnerUuid = DefaultLayerUuid();
  // Only the known private legacy token gets this special deterministic policy.
  if (auto it = scene.layers.find("layer_default"); it != scene.layers.end()) {
    Layer legacy = it->second;
    scene.layers.erase(it);
    legacy.uuid = CanonicalizeUuid(legacy.uuid);
    if (legacy.uuid.empty()) legacy.uuid = DefaultLayerUuid();
    legacyOwnerUuid = legacy.uuid;
    auto [target, inserted] = scene.layers.try_emplace(legacy.uuid, legacy);
    if (!inserted)
      target->second.childUUIDs.insert(target->second.childUUIDs.end(),
                                     legacy.childUUIDs.begin(), legacy.childUUIDs.end());
  }
  std::map<std::string, std::string> childOwners;
  // A persisted UUID or explicit child ownership outranks a legacy name.
  for (const auto &[uuid, layer] : scene.layers)
    for (const auto &child : layer.childUUIDs) {
      auto [it, inserted] = childOwners.emplace(child, uuid);
      if (!inserted && it->second != uuid) it->second.clear();
    }
  std::map<std::string, std::string> inferredOwners;
  VisitObjects(scene, [&](const auto &uuid, auto &object) {
    if (object.layerUuid == "layer_default") object.layerUuid = legacyOwnerUuid;
    if (!object.layerUuid.empty()) {
      if (object.layerUuid == DefaultLayerUuid()) EnsureDefaultLayer(scene);
      return;
    }
    if (auto it = childOwners.find(uuid); it != childOwners.end())
      object.layerUuid = it->second;
    if (object.layerUuid.empty())
      object.layerUuid = ResolveLayerReference(scene, object.layer);
    if (!object.layerUuid.empty()) return;
    if (object.layer.empty() || object.layer == kDefaultName ||
        object.layer == "layer_default") {
      object.layerUuid = EnsureDefaultLayer(scene).uuid;
      return;
    }
    // Legacy names with no ownership cannot identify one of several layers.
    // Keep their objects in a distinct inferred layer rather than merging UUIDs.
    auto [owner, inserted] = inferredOwners.try_emplace(object.layer);
    if (inserted) {
      const auto seed = "mvr:layer:inferred:" + object.layer;
      owner->second = DeriveDeterministicUuid(seed);
      for (size_t suffix = 0; scene.layers.contains(owner->second); ++suffix)
        owner->second = DeriveDeterministicUuid(seed + "#" + std::to_string(suffix));
      scene.layers.emplace(owner->second, Layer{owner->second, object.layer, {}, {}});
    }
    object.layerUuid = owner->second;
  });
  RefreshLayerLabels(scene);
}
} // namespace layerdomain
