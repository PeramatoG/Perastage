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
  case RecoveryReason::AmbiguousReference:
    return "ambiguous-reference";
  case RecoveryReason::UnresolvedReference:
    return "unresolved-reference";
  }
  return "unknown";
}

// Escapes control and delimiter characters in structured identity diagnostics.
std::string EscapeDiagnosticText(const std::string &text) {
  std::ostringstream out;
  static constexpr char kHex[] = "0123456789abcdef";
  for (const unsigned char character : text) {
    if (character == '\\')
      out << "\\\\";
    else if (character == '"')
      out << "\\\"";
    else if (character >= 0x20 && character != 0x7f)
      out << static_cast<char>(character);
    else
      out << "\\x" << kHex[character >> 4] << kHex[character & 0x0f];
  }
  return out.str();
}

// Appends one material recovery diagnostic with explicit identity ownership.
void AddDiagnostic(RecoveryResult &result, const std::string &kind,
                   const std::string &name, const std::string &context,
                   const std::string &identitySource,
                   const std::string &original, const std::string &replacement,
                   RecoveryReason reason) {
  result.diagnostics.push_back(
      {kind, name, context, identitySource, original, replacement, reason});
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

// Resolves exact and canonical legacy aliases without guessing collisions.
class IdentityAliasResolver {
public:
  enum class Resolution { Resolved, Ambiguous, Unresolved };

  struct Result {
    Resolution resolution = Resolution::Unresolved;
    std::string target;
  };

  // Registers exact and canonical forms of one non-empty identity alias.
  void Register(const std::string &alias, const std::string &target) {
    if (alias.empty())
      return;
    aliases[alias].insert(target);
    const std::string canonical = CanonicalizeUuid(alias);
    if (!canonical.empty())
      aliases[canonical].insert(target);
  }

  // Resolves an exact alias first and then its canonical spelling.
  Result Resolve(const std::string &reference) const {
    if (reference.empty())
      return {};
    const auto exact = aliases.find(reference);
    if (exact != aliases.end())
      return ResolveTargets(exact->second);
    const std::string canonical = CanonicalizeUuid(reference);
    if (canonical.empty())
      return {};
    const auto normalized = aliases.find(canonical);
    return normalized == aliases.end() ? Result{}
                                       : ResolveTargets(normalized->second);
  }

private:
  // Converts an alias target set into an ambiguity-safe result.
  static Result ResolveTargets(const std::set<std::string> &targets) {
    if (targets.size() == 1)
      return {Resolution::Resolved, *targets.begin()};
    if (targets.size() > 1)
      return {Resolution::Ambiguous, {}};
    return {};
  }

  std::map<std::string, std::set<std::string>> aliases;
};

// Reports all field and key problems without hiding malformed identity causes.
void DiagnoseIdentityInputs(RecoveryResult &result, const std::string &kind,
                            const std::string &name, const std::string &context,
                            const std::string &key, const std::string &field,
                            const std::string &replacement) {
  const std::string canonicalKey = CanonicalizeUuid(key);
  const std::string canonicalField = CanonicalizeUuid(field);
  if (key.empty())
    AddDiagnostic(result, kind, name, context, "map-key", key, replacement,
                  RecoveryReason::Missing);
  else if (canonicalKey.empty())
    AddDiagnostic(result, kind, name, context, "map-key", key, replacement,
                  RecoveryReason::Malformed);
  else if (canonicalKey != key)
    AddDiagnostic(result, kind, name, context, "map-key", key, replacement,
                  RecoveryReason::Canonicalized);

  if (field.empty())
    AddDiagnostic(result, kind, name, context, "object-field", field,
                  replacement, RecoveryReason::Missing);
  else if (canonicalField.empty())
    AddDiagnostic(result, kind, name, context, "object-field", field,
                  replacement, RecoveryReason::Malformed);
  else if (canonicalField != field)
    AddDiagnostic(result, kind, name, context, "object-field", field,
                  replacement, RecoveryReason::Canonicalized);

  if (!key.empty() && !field.empty() &&
      (canonicalKey.empty() || canonicalField.empty() ||
       canonicalKey != canonicalField)) {
    AddDiagnostic(result, kind, name, context, "map-key/object-field",
                  key + " | " + field, replacement,
                  RecoveryReason::KeyFieldMismatch);
  }
}

// Repairs one object map and registers every source spelling as an alias.
template <typename Object>
void RecoverObjectMap(std::unordered_map<std::string, Object> &objects,
                      const std::string &kind, const std::string &context,
                      std::set<std::string> &used,
                      IdentityAliasResolver &aliases, RecoveryResult &result) {
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
    const std::string field = object.uuid;
    const std::string canonicalKey = CanonicalizeUuid(key);
    const std::string canonicalField = CanonicalizeUuid(field);
    std::string replacement =
        !canonicalField.empty() ? canonicalField : canonicalKey;
    if (replacement.empty() || used.contains(replacement)) {
      if (!replacement.empty())
        AddDiagnostic(result, kind, ObjectName(object), context,
                      "canonical-identity", replacement, {},
                      RecoveryReason::Duplicate);
      replacement = DeriveUnique("mvr:identity:" + context + ":" + kind + ":" +
                                     key + ":" + field,
                                 used);
    }

    DiagnoseIdentityInputs(result, kind, ObjectName(object), context, key,
                           field, replacement);
    used.insert(replacement);
    aliases.Register(key, replacement);
    aliases.Register(field, replacement);
    aliases.Register(replacement, replacement);
    object.uuid = replacement;
    recovered.emplace(replacement, std::move(object));
  }
  objects = std::move(recovered);
}

// Rewrites one scene-object reference or clears it after a diagnosed failure.
void RewriteReference(std::string &reference, const std::string &ownerKind,
                      const std::string &ownerName,
                      const std::string &referenceKind,
                      const std::string &sourceContext,
                      const IdentityAliasResolver &aliases,
                      RecoveryResult &result) {
  if (reference.empty())
    return;
  const std::string original = reference;
  const auto resolved = aliases.Resolve(reference);
  if (resolved.resolution == IdentityAliasResolver::Resolution::Resolved) {
    reference = resolved.target;
    return;
  }
  reference.clear();
  AddDiagnostic(
      result, ownerKind, ownerName, sourceContext, referenceKind, original, {},
      resolved.resolution == IdentityAliasResolver::Resolution::Ambiguous
          ? RecoveryReason::AmbiguousReference
          : RecoveryReason::UnresolvedReference);
}

// Recovers layer map keys and fields without merging layers by display name.
void RecoverLayers(MvrScene &scene, const std::string &sourceContext,
                   RecoveryResult &result) {
  std::vector<std::string> keys;
  keys.reserve(scene.layers.size());
  for (const auto &[key, layer] : scene.layers) {
    (void)layer;
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end());

  std::vector<Layer> layers;
  std::set<std::string> names;
  std::set<std::string> used;
  for (const std::string &key : keys) {
    Layer layer = scene.layers.at(key);
    names.insert(layer.name);
    const std::string field = layer.uuid;
    const std::string canonicalKey = CanonicalizeUuid(key);
    const std::string canonicalField = CanonicalizeUuid(field);
    std::string replacement =
        !canonicalField.empty() ? canonicalField : canonicalKey;
    if (replacement.empty() || used.contains(replacement)) {
      if (!replacement.empty())
        AddDiagnostic(result, "Layer", layer.name, sourceContext,
                      "canonical-identity", replacement, {},
                      RecoveryReason::Duplicate);
      replacement = DeriveUnique("mvr:identity:" + sourceContext +
                                     ":Layer:" + key + ":" + field,
                                 used);
    }
    DiagnoseIdentityInputs(result, "Layer", layer.name, sourceContext, key,
                           field, replacement);
    used.insert(replacement);
    layer.uuid = replacement;
    layers.push_back(std::move(layer));
  }

  auto infer = [&](const std::string &rawName) {
    const std::string name = rawName.empty() ? DEFAULT_LAYER_NAME : rawName;
    if (names.insert(name).second) {
      Layer layer;
      layer.name = name;
      layer.uuid = DeriveUnique(
          "mvr:identity:" + sourceContext + ":Layer:inferred:" + name, used);
      used.insert(layer.uuid);
      AddDiagnostic(result, "Layer", name, sourceContext, "object-assignment",
                    {}, layer.uuid, RecoveryReason::InferredLayer);
      layers.push_back(std::move(layer));
    }
  };
  infer(DEFAULT_LAYER_NAME);
  for (const auto &[uuid, object] : scene.fixtures) {
    (void)uuid;
    infer(object.layer);
  }
  for (const auto &[uuid, object] : scene.trusses) {
    (void)uuid;
    infer(object.layer);
  }
  for (const auto &[uuid, object] : scene.supports) {
    (void)uuid;
    infer(object.layer);
  }
  for (const auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    infer(object.layer);
  }
  for (const auto &[uuid, object] : scene.groupObjects) {
    (void)uuid;
    infer(object.layer);
  }

  scene.layers.clear();
  for (Layer &layer : layers)
    scene.layers.emplace(layer.uuid, std::move(layer));
}

} // namespace

// Canonicalizes and deterministically repairs all persisted scene identities.
RecoveryResult RecoverSceneIdentities(MvrScene &scene,
                                      const std::string &sourceContext) {
  RecoveryResult result;
  std::set<std::string> usedSceneUuids;
  IdentityAliasResolver aliases;

  RecoverObjectMap(scene.fixtures, "Fixture", sourceContext, usedSceneUuids,
                   aliases, result);
  RecoverObjectMap(scene.trusses, "Truss", sourceContext, usedSceneUuids,
                   aliases, result);
  RecoverObjectMap(scene.supports, "Support", sourceContext, usedSceneUuids,
                   aliases, result);
  RecoverObjectMap(scene.sceneObjects, "SceneObject", sourceContext,
                   usedSceneUuids, aliases, result);
  RecoverObjectMap(scene.groupObjects, "GroupObject", sourceContext,
                   usedSceneUuids, aliases, result);

  for (auto &[uuid, object] : scene.fixtures) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, "Fixture", object.instanceName,
                     "parent-group", sourceContext, aliases, result);
  }
  for (auto &[uuid, object] : scene.trusses) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, "Truss", object.name,
                     "parent-group", sourceContext, aliases, result);
  }
  for (auto &[uuid, object] : scene.supports) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, "Support", object.name,
                     "parent-group", sourceContext, aliases, result);
    RewriteReference(object.motorFixtureUuid, "Support", object.name,
                     "motor-fixture", sourceContext, aliases, result);
  }
  for (auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    RewriteReference(object.parentGroupUuid, "SceneObject", object.name,
                     "parent-group", sourceContext, aliases, result);
  }
  for (auto &[uuid, group] : scene.groupObjects) {
    (void)uuid;
    RewriteReference(group.parentGroupUuid, "GroupObject", group.name,
                     "parent-group", sourceContext, aliases, result);
    for (auto &child : group.children)
      RewriteReference(child.uuid, "GroupObject", group.name, "child",
                       sourceContext, aliases, result);
  }
  for (auto &[uuid, layer] : scene.layers) {
    (void)uuid;
    for (std::string &childUuid : layer.childUUIDs)
      RewriteReference(childUuid, "Layer", layer.name, "child", sourceContext,
                       aliases, result);
    layer.childUUIDs.erase(
        std::remove(layer.childUUIDs.begin(), layer.childUUIDs.end(), ""),
        layer.childUUIDs.end());
  }

  RecoverLayers(scene, sourceContext, result);
  return result;
}

// Formats a recovery record as a stable structured diagnostic.
std::string FormatRecoveryDiagnostic(const RecoveryDiagnostic &diagnostic) {
  std::ostringstream out;
  out << "mvr_identity_recovery kind=\""
      << EscapeDiagnosticText(diagnostic.objectKind) << "\" name=\""
      << EscapeDiagnosticText(diagnostic.objectName) << "\" source=\""
      << EscapeDiagnosticText(diagnostic.sourceContext)
      << "\" identity_source=\""
      << EscapeDiagnosticText(diagnostic.identitySource) << "\" original=\""
      << EscapeDiagnosticText(diagnostic.originalIdentity)
      << "\" replacement=\""
      << EscapeDiagnosticText(diagnostic.replacementIdentity)
      << "\" reason=" << ReasonName(diagnostic.reason);
  return out.str();
}

} // namespace mvridentity
