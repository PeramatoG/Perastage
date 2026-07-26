#include "mvr_identity_recovery.h"
#include "uuidutils.h"

#include <cassert>
#include <string>
#include <unordered_set>

namespace {

constexpr const char *kFixtureUuid = "11111111-1111-4111-8111-111111111111";
constexpr const char *kTrussUuid = "22222222-2222-4222-8222-222222222222";
constexpr const char *kSupportUuid = "33333333-3333-4333-8333-333333333333";
constexpr const char *kSceneObjectUuid = "44444444-4444-4444-8444-444444444444";
constexpr const char *kGroupUuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
constexpr const char *kLayerUuid = "55555555-5555-4555-8555-555555555555";

// Returns whether a recovery result contains the requested reason.
bool HasReason(const mvridentity::RecoveryResult &result,
               mvridentity::RecoveryReason reason) {
  for (const auto &diagnostic : result.diagnostics) {
    if (diagnostic.reason == reason)
      return true;
  }
  return false;
}

// Verifies every object map stores the same canonical UUID in key and field.
template <typename Object>
void AssertCanonicalMap(
    const std::unordered_map<std::string, Object> &objects) {
  for (const auto &[key, object] : objects) {
    assert(key == object.uuid);
    assert(CanonicalizeUuid(key) == key);
  }
}

// Verifies canonical spellings remain unchanged across every object map.
void TestCanonicalIdentityPreservation() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = kFixtureUuid;
  scene.fixtures[fixture.uuid] = fixture;
  Truss truss;
  truss.uuid = kTrussUuid;
  scene.trusses[truss.uuid] = truss;
  Support support;
  support.uuid = kSupportUuid;
  support.motorFixtureUuid = kFixtureUuid;
  scene.supports[support.uuid] = support;
  SceneObject object;
  object.uuid = kSceneObjectUuid;
  scene.sceneObjects[object.uuid] = object;
  GroupObject group;
  group.uuid = kGroupUuid;
  scene.groupObjects[group.uuid] = group;
  Layer layer;
  layer.uuid = kLayerUuid;
  layer.name = "Imported";
  scene.layers[layer.uuid] = layer;

  const auto first =
      mvridentity::RecoverSceneIdentities(scene, "canonical-preservation");
  assert(scene.fixtures.contains(kFixtureUuid));
  assert(scene.trusses.contains(kTrussUuid));
  assert(scene.supports.contains(kSupportUuid));
  assert(scene.sceneObjects.contains(kSceneObjectUuid));
  assert(scene.groupObjects.contains(kGroupUuid));
  assert(scene.layers.contains(kLayerUuid));
  assert(scene.supports.at(kSupportUuid).motorFixtureUuid == kFixtureUuid);
  assert(!HasReason(first, mvridentity::RecoveryReason::Malformed));

  const auto second =
      mvridentity::RecoverSceneIdentities(scene, "canonical-preservation");
  assert(second.diagnostics.empty());
}

// Verifies all tolerant spellings resolve through hierarchy and motor aliases.
void TestSpellingsAndReferences() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "11111111111141118111111111111111";
  fixture.instanceName = "Motor Fixture";
  scene.fixtures[" {11111111-1111-4111-8111-111111111111} "] = fixture;

  GroupObject group;
  group.uuid = " {AAAAAAAA-BBBB-4CCC-8DDD-EEEEEEEEEEEE} ";
  group.name = "Group";
  group.layer = "Rig";
  scene.groupObjects["legacy-group-key"] = group;

  Truss truss;
  truss.uuid = "broken-truss";
  truss.name = "Truss";
  truss.layer = "Rig";
  truss.parentGroupUuid = "AAAAAAAA-BBBB-4CCC-8DDD-EEEEEEEEEEEE";
  scene.trusses[truss.uuid] = truss;
  scene.groupObjects["legacy-group-key"].children = {
      {MvrNodeType::Truss, "broken-truss"}, {MvrNodeType::Truss, truss.uuid}};

  Support support;
  support.uuid.clear();
  support.name = "Hoist";
  support.layer = "Rig";
  support.parentGroupUuid = " {AAAAAAAA-BBBB-4CCC-8DDD-EEEEEEEEEEEE} ";
  support.motorFixtureUuid = "11111111-1111-4111-8111-111111111111";
  scene.supports["legacy-support-key"] = support;

  Layer malformedLayer;
  malformedLayer.uuid = "layer1";
  malformedLayer.name = "Rig";
  malformedLayer.childUUIDs = {"broken-truss"};
  scene.layers["legacy-layer-key"] = malformedLayer;

  const auto first =
      mvridentity::RecoverSceneIdentities(scene, "spelling-references");
  assert(HasReason(first, mvridentity::RecoveryReason::Canonicalized));
  assert(HasReason(first, mvridentity::RecoveryReason::Malformed));
  assert(HasReason(first, mvridentity::RecoveryReason::Missing));
  assert(HasReason(first, mvridentity::RecoveryReason::KeyFieldMismatch));
  assert(scene.groupObjects.contains(kGroupUuid));
  const auto &recoveredTruss = scene.trusses.begin()->second;
  const auto &recoveredSupport = scene.supports.begin()->second;
  const auto &recoveredGroup = scene.groupObjects.at(kGroupUuid);
  assert(recoveredTruss.parentGroupUuid == kGroupUuid);
  assert(recoveredSupport.parentGroupUuid == kGroupUuid);
  assert(recoveredSupport.motorFixtureUuid == kFixtureUuid);
  assert(recoveredGroup.children.at(0).uuid == recoveredTruss.uuid);
  assert(recoveredGroup.children.at(1).uuid == recoveredTruss.uuid);
  assert(scene.layers.size() == 2);
  for (const auto &[key, recoveredLayer] : scene.layers) {
    assert(key == recoveredLayer.uuid);
    assert(CanonicalizeUuid(key) == key);
    for (const std::string &child : recoveredLayer.childUUIDs)
      assert(scene.trusses.contains(child));
  }

  AssertCanonicalMap(scene.fixtures);
  AssertCanonicalMap(scene.trusses);
  AssertCanonicalMap(scene.supports);
  AssertCanonicalMap(scene.groupObjects);
  const std::string stableTrussUuid = recoveredTruss.uuid;
  const auto second =
      mvridentity::RecoverSceneIdentities(scene, "spelling-references");
  assert(second.diagnostics.empty());
  assert(scene.trusses.contains(stableTrussUuid));
}

// Verifies duplicate aliases are diagnosed and never guessed for references.
void TestDuplicateAndAmbiguousAliases() {
  MvrScene scene;
  Fixture first;
  first.uuid = kFixtureUuid;
  first.instanceName = "First";
  scene.fixtures["fixture-a"] = first;
  Fixture second = first;
  second.instanceName = "Second";
  scene.fixtures["fixture-b"] = second;
  Truss crossKind;
  crossKind.uuid = kFixtureUuid;
  crossKind.name = "Cross-kind duplicate";
  scene.trusses["truss-key"] = crossKind;

  Support support;
  support.uuid = kSupportUuid;
  support.motorFixtureUuid = " {11111111-1111-4111-8111-111111111111} ";
  scene.supports[support.uuid] = support;

  const auto firstResult =
      mvridentity::RecoverSceneIdentities(scene, "duplicate-aliases");
  assert(HasReason(firstResult, mvridentity::RecoveryReason::Duplicate));
  assert(
      HasReason(firstResult, mvridentity::RecoveryReason::AmbiguousReference));
  assert(scene.supports.at(kSupportUuid).motorFixtureUuid.empty());
  assert(scene.fixtures.size() == 2);
  assert(scene.trusses.size() == 1);
  AssertCanonicalMap(scene.fixtures);
  AssertCanonicalMap(scene.trusses);

  const auto secondResult =
      mvridentity::RecoverSceneIdentities(scene, "duplicate-aliases");
  assert(secondResult.diagnostics.empty());
}

// Verifies valid-key conflicts, malformed fields, and missing fields recover
// distinctly.
void TestKeyFieldConflictMatrix() {
  MvrScene scene;
  SceneObject validConflict;
  validConflict.uuid = kSceneObjectUuid;
  scene.sceneObjects[kTrussUuid] = validConflict;
  SceneObject malformedField;
  malformedField.uuid = "bad-field";
  scene.sceneObjects[kSupportUuid] = malformedField;
  SceneObject missingField;
  scene.sceneObjects[kFixtureUuid] = missingField;

  const auto first =
      mvridentity::RecoverSceneIdentities(scene, "key-field-matrix");
  assert(HasReason(first, mvridentity::RecoveryReason::Malformed));
  assert(HasReason(first, mvridentity::RecoveryReason::Missing));
  assert(HasReason(first, mvridentity::RecoveryReason::KeyFieldMismatch));
  assert(scene.sceneObjects.contains(kSceneObjectUuid));
  assert(scene.sceneObjects.contains(kSupportUuid));
  assert(scene.sceneObjects.contains(kFixtureUuid));
  AssertCanonicalMap(scene.sceneObjects);

  const auto second =
      mvridentity::RecoverSceneIdentities(scene, "key-field-matrix");
  assert(second.diagnostics.empty());
}

// Verifies layer duplicates recover deterministically and identity survives
// rename.
void TestLayerIdentityMatrix() {
  MvrScene scene;
  Layer first;
  first.uuid = kLayerUuid;
  first.name = "Layer A";
  scene.layers["layer-key-a"] = first;
  Layer duplicate;
  duplicate.uuid = kLayerUuid;
  duplicate.name = "Layer B";
  scene.layers["layer-key-b"] = duplicate;

  const auto firstResult =
      mvridentity::RecoverSceneIdentities(scene, "layer-matrix");
  assert(HasReason(firstResult, mvridentity::RecoveryReason::Duplicate));
  assert(scene.layers.size() == 3);
  for (const auto &[key, layer] : scene.layers) {
    assert(key == layer.uuid);
    assert(CanonicalizeUuid(key) == key);
  }
  const auto imported = scene.layers.find(kLayerUuid);
  assert(imported != scene.layers.end());
  const std::string preserved = imported->first;
  imported->second.name = "Renamed Layer";

  const auto secondResult =
      mvridentity::RecoverSceneIdentities(scene, "layer-matrix");
  assert(secondResult.diagnostics.empty());
  assert(scene.layers.contains(preserved));
  assert(scene.layers.at(preserved).name == "Renamed Layer");
}

// Verifies unresolved references are diagnosed, cleared, and safely escaped.
void TestUnresolvedReferenceDiagnostic() {
  MvrScene scene;
  Support support;
  support.uuid = kSupportUuid;
  support.name = "Hoist\"\nName";
  support.motorFixtureUuid = "missing\nfixture";
  scene.supports[support.uuid] = support;

  const auto first =
      mvridentity::RecoverSceneIdentities(scene, "diagnostic\nsource");
  assert(HasReason(first, mvridentity::RecoveryReason::UnresolvedReference));
  assert(scene.supports.at(kSupportUuid).motorFixtureUuid.empty());
  std::string formatted;
  for (const auto &diagnostic : first.diagnostics) {
    if (diagnostic.reason == mvridentity::RecoveryReason::UnresolvedReference)
      formatted = mvridentity::FormatRecoveryDiagnostic(diagnostic);
  }
  assert(formatted.find("Hoist\\\"\\x0aName") != std::string::npos);
  assert(formatted.find("missing\\x0afixture") != std::string::npos);
  assert(formatted.find('\n') == std::string::npos);

  const auto second =
      mvridentity::RecoverSceneIdentities(scene, "diagnostic\nsource");
  assert(second.diagnostics.empty());
}

// Returns the stable replacement produced for the same damaged scene.
std::string RecoverDeterministicTrussUuid() {
  MvrScene scene;
  Truss truss;
  truss.uuid = "malformed";
  scene.trusses["legacy-key"] = truss;
  mvridentity::RecoverSceneIdentities(scene, "deterministic-recovery");
  return scene.trusses.begin()->first;
}

} // namespace

// Runs the focused MVR identity and dependent-reference recovery matrix.
int main() {
  TestCanonicalIdentityPreservation();
  TestSpellingsAndReferences();
  TestDuplicateAndAmbiguousAliases();
  TestKeyFieldConflictMatrix();
  TestLayerIdentityMatrix();
  TestUnresolvedReferenceDiagnostic();
  assert(RecoverDeterministicTrussUuid() == RecoverDeterministicTrussUuid());
  return 0;
}
