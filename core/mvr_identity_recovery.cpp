#include "mvr_identity_recovery.h"

#include "configservices.h"
#include "uuidutils.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace mvridentity {
namespace {

// Returns the display name used when diagnosing a fixture identity.
std::string ObjectName(const Fixture &object) { return object.instanceName; }

// Returns the display name used when diagnosing other scene identities.
template <typename Object> std::string ObjectName(const Object &object) {
  return object.name;
}

// Returns the stable diagnostic spelling for a recovery reason.
const char *ReasonName(RecoveryReason reason) {
  switch (reason) {
  case RecoveryReason::Canonicalized:
    return "canonicalized";
  case RecoveryReason::Missing:
    return "missing";
  case RecoveryReason::Malformed:
    return "malformed";
  case RecoveryReason::Duplicate:
    return "duplicate";
  case RecoveryReason::KeyFieldMismatch:
    return "key-field-mismatch";
  case RecoveryReason::InferredLayer:
    return "inferred-layer";
  }
  return "unknown";
}

// Derives a collision-free deterministic replacement within one identity scope.
std::string DeriveUnique(const std::string &seed,
                         const std::set<std::string> &used) {
  for (std::size_t suffix = 0;; ++suffix) {
    const std::string candidate =
        DeriveDeterministicUuid(seed + "#" + std::to_string(suffix));
    if (!used.contains(candidate))
      return candidate;
  }
}

// Repairs one object map while preserving a mapping for hierarchy references.
template <typename Object>
void RecoverObjectMap(std::unordered_map<std::string, Object> &objects,
                      const std::string &kind, const std::string &context,
                      std::set<std::string> &used,
                      std::unordered_map<std::string, std::string> &rewrites,
                      RecoveryResult &result) {
  std::vector<std::string> keys;
  keys.reserve(objects.size());
  for (const auto &[key, object] : objects) {
    (void)object;
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end());

  std::unordered_map<std::string, Object> recovered;
  for (const std::string &key : keys) {
    Object object = objects.at(key);
    const std::string raw = object.uuid.empty() ? key : object.uuid;
    const std::string canonical = CanonicalizeUuid(raw);
    std::string replacement = canonical;
    RecoveryReason reason = RecoveryReason::Canonicalized;
    bool report = false;

    if (raw.empty()) {
      reason = RecoveryReason::Missing;
      report = true;
    } else if (canonical.empty()) {
      reason = RecoveryReason::Malformed;
      report = true;
    } else if (used.contains(canonical)) {
      reason = RecoveryReason::Duplicate;
      report = true;
      replacement.clear();
    } else if (canonical != raw) {
      report = true;
    }
    if (!key.empty() && !object.uuid.empty() && key != object.uuid) {
      reason = RecoveryReason::KeyFieldMismatch;
      report = true;
    }
    if (replacement.empty() || used.contains(replacement)) {
      replacement =
          DeriveUnique("mvr:identity:" + context + ":" + kind + ":" + key +
                           ":" + object.uuid + ":" + ObjectName(object),
                       used);
    }

    used.insert(replacement);
    if (!key.empty())
      rewrites[key] = replacement;
    if (!object.uuid.empty() && !rewrites.contains(object.uuid))
      rewrites[object.uuid] = replacement;
    object.uuid = replacement;
    recovered.emplace(replacement, std::move(object));
    if (report) {
      result.diagnostics.push_back({kind, ObjectName(recovered.at(replacement)),
                                    context, raw, replacement, reason});
    }
  }
  objects = std::move(recovered);
}

// Rewrites one optional scene-object reference through the transaction map.
void RewriteReference(std::string &reference,
                      const std::unordered_map<std::string, std::string> &map) {
  const auto found = map.find(reference);
  if (found != map.end())
    reference = found->second;
}

} // namespace

// Canonicalizes and deterministically repairs all persisted scene identities.
RecoveryResult RecoverSceneIdentities(MvrScene &scene,
                                      const std::string &sourceContext) {
  RecoveryResult result;
  std::set<std::string> usedSceneUuids;
  std::unordered_map<std::string, std::string> objectRewrites;

  RecoverObjectMap(scene.fixtures, "Fixture", sourceContext, usedSceneUuids,
                   objectRewrites, result);
  RecoverObjectMap(scene.trusses, "Truss", sourceContext, usedSceneUuids,
                   objectRewrites, result);
  RecoverObjectMap(scene.supports, "Support", sourceContext, usedSceneUuids,
                   objectRewrites, result);
  RecoverObjectMap(scene.sceneObjects, "SceneObject", sourceContext,
                   usedSceneUuids, objectRewrites, result);
  RecoverObjectMap(scene.groupObjects, "GroupObject", sourceContext,
                   usedSceneUuids, objectRewrites, result);

  for (auto &[uuid, object] : scene.fixtures) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, objectRewrites);
  }
  for (auto &[uuid, object] : scene.trusses) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, objectRewrites);
  }
  for (auto &[uuid, object] : scene.supports) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, objectRewrites);
  }
  for (auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, objectRewrites);
  }
  for (auto &[uuid, group] : scene.groupObjects) {
    (void)uuid;
    RewriteReference(group.parentGroupUuid, objectRewrites);
    for (auto &child : group.children)
      RewriteReference(child.uuid, objectRewrites);
  }

  std::vector<Layer> recoveredLayers;
  std::set<std::string> existingLayerNames;
  for (const auto &[key, layer] : scene.layers) {
    Layer copy = layer;
    if (copy.uuid.empty())
      copy.uuid = key;
    existingLayerNames.insert(copy.name);
    recoveredLayers.push_back(std::move(copy));
  }
  auto inferLayer = [&](const std::string &name) {
    const std::string effective = name.empty() ? DEFAULT_LAYER_NAME : name;
    if (!existingLayerNames.contains(effective)) {
      Layer layer;
      layer.name = effective;
      recoveredLayers.push_back(std::move(layer));
      existingLayerNames.insert(effective);
    }
  };
  inferLayer(DEFAULT_LAYER_NAME);
  for (const auto &[uuid, object] : scene.fixtures) {
    (void)uuid;
    inferLayer(object.layer);
  }
  for (const auto &[uuid, object] : scene.trusses) {
    (void)uuid;
    inferLayer(object.layer);
  }
  for (const auto &[uuid, object] : scene.supports) {
    (void)uuid;
    inferLayer(object.layer);
  }
  for (const auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    inferLayer(object.layer);
  }
  for (const auto &[uuid, object] : scene.groupObjects) {
    (void)uuid;
    inferLayer(object.layer);
  }

  scene.layers.clear();
  std::sort(recoveredLayers.begin(), recoveredLayers.end(),
            [](const Layer &left, const Layer &right) {
              if (left.name != right.name)
                return left.name < right.name;
              return left.uuid < right.uuid;
            });
  std::set<std::string> usedLayerUuids;
  for (auto &layer : recoveredLayers) {
    const std::string &name = layer.name;
    const std::string raw = layer.uuid;
    const std::string canonical = CanonicalizeUuid(raw);
    std::string replacement = canonical;
    RecoveryReason reason = RecoveryReason::Canonicalized;
    bool report = canonical != raw;
    if (raw.empty()) {
      reason = RecoveryReason::InferredLayer;
      report = true;
    } else if (canonical.empty()) {
      reason = RecoveryReason::Malformed;
      report = true;
    } else if (usedLayerUuids.contains(canonical)) {
      reason = RecoveryReason::Duplicate;
      replacement.clear();
      report = true;
    }
    if (replacement.empty()) {
      replacement = DeriveUnique("mvr:identity:" + sourceContext +
                                     ":Layer:" + raw + ":" + name,
                                 usedLayerUuids);
    }
    usedLayerUuids.insert(replacement);
    layer.uuid = replacement;
    scene.layers.emplace(replacement, layer);
    if (report)
      result.diagnostics.push_back(
          {"Layer", name, sourceContext, raw, replacement, reason});
  }
  return result;
}

// Formats a recovery record as a stable structured diagnostic.
std::string FormatRecoveryDiagnostic(const RecoveryDiagnostic &diagnostic) {
  std::ostringstream out;
  out << "mvr_identity_recovery kind=" << diagnostic.objectKind << " name=\""
      << diagnostic.objectName << "\" source=\"" << diagnostic.sourceContext
      << "\" original=\"" << diagnostic.originalIdentity << "\" replacement=\""
      << diagnostic.replacementIdentity
      << "\" reason=" << ReasonName(diagnostic.reason);
  return out.str();
}

} // namespace mvridentity
