#include "mvr_export_preparation.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace {

// Builds a fixture with a stable identity for preparation characterization.
Fixture MakeFixture(const std::string &uuid, const std::string &name,
                    int fixtureId, int unitNumber, float x) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.instanceName = name;
  fixture.typeName = "Wash";
  fixture.fixtureId = fixtureId;
  fixture.fixtureIdNumeric = fixtureId;
  fixture.unitNumber = unitNumber;
  fixture.transform.o[0] = x;
  return fixture;
}

// Finds a diagnostic with the requested code in one preparation phase.
const MvrExportDiagnostic *
FindDiagnostic(const std::vector<MvrExportDiagnostic> &diagnostics,
               MvrExportDiagnosticCode code) {
  const auto found =
      std::find_if(diagnostics.begin(), diagnostics.end(),
                   [code](const MvrExportDiagnostic &diagnostic) {
                     return diagnostic.code == code;
                   });
  return found != diagnostics.end() ? &*found : nullptr;
}

// Verifies fixture identifiers, UnitNumbers, layers, and immutable input
// behavior.
void TestPreparedValuesAndSourceImmutability() {
  const std::string firstUuid = "11111111-1111-4111-8111-111111111111";
  const std::string secondUuid = "22222222-2222-4222-8222-222222222222";
  const std::string thirdUuid = "33333333-3333-4333-8333-333333333333";
  MvrScene scene;
  scene.fixtures.emplace(firstUuid,
                         MakeFixture(firstUuid, "First", 7, 5, 3.0f));
  scene.fixtures.emplace(secondUuid,
                         MakeFixture(secondUuid, "Second", 7, 0, 1.0f));
  scene.fixtures.emplace(thirdUuid,
                         MakeFixture(thirdUuid, "Third", 0, 0, 2.0f));
  scene.fixtures.at(firstUuid).fixtureIdText = "House 7";
  Layer layer;
  layer.uuid = "legacy-layer";
  layer.name = "Rig";
  scene.layers.emplace(layer.uuid, layer);

  const MvrScene original = scene;
  const auto result =
      mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());

  assert(result.success);
  assert(result.objectIds.at(firstUuid).first == "House 7");
  assert(result.objectIds.at(firstUuid).second == 7);
  assert(result.objectIds.at(secondUuid).second > 0);
  assert(result.objectIds.at(secondUuid).second != 7);
  assert(result.objectIds.at(thirdUuid).second > 0);
  assert(result.fixtureUnitNumbers.at(firstUuid) == 5);
  assert(result.fixtureUnitNumbers.at(secondUuid) == 1);
  assert(result.fixtureUnitNumbers.at(thirdUuid) == 2);
  bool foundRigLayer = false;
  for (const auto &[uuid, preparedLayer] : result.scene.layers) {
    if (preparedLayer.name == "Rig") {
      foundRigLayer = true;
      assert(result.layerUuids.at(uuid).size() == 36);
    }
  }
  assert(foundRigLayer);
  assert(scene.fixtures.at(secondUuid).fixtureId ==
         original.fixtures.at(secondUuid).fixtureId);
  assert(scene.fixtures.at(secondUuid).unitNumber ==
         original.fixtures.at(secondUuid).unitNumber);
  assert(scene.layers.contains("legacy-layer"));
  const MvrExportDiagnostic *fixtureIdDiagnostic = FindDiagnostic(
      result.objectIdDiagnostics, MvrExportDiagnosticCode::FixtureIdReassigned);
  assert(fixtureIdDiagnostic != nullptr);
  assert(fixtureIdDiagnostic->severity == MvrExportDiagnosticSeverity::Warning);
  assert(fixtureIdDiagnostic->impact ==
         MvrExportDiagnosticImpact::IdentityChanged);
  assert(fixtureIdDiagnostic->userVisible);
}

// Verifies Position normalization, reference resolution, and diagnostics.
void TestPositionPreparation() {
  const std::string canonicalPosition = "44444444-4444-4444-8444-444444444444";
  const std::string fixtureUuid = "55555555-5555-4555-8555-555555555555";
  const std::string trussUuid = "66666666-6666-4666-8666-666666666666";
  const std::string supportUuid = "77777777-7777-4777-8777-777777777777";
  const std::string unresolvedUuid = "88888888-8888-4888-8888-888888888888";
  MvrScene scene;
  scene.positions.emplace(canonicalPosition, "Canonical Position");
  scene.positions.emplace("legacy-position", "Legacy Position");

  Fixture fixture = MakeFixture(fixtureUuid, "Fixture", 1, 1, 0.0f);
  fixture.position = canonicalPosition;
  scene.fixtures.emplace(fixtureUuid, fixture);
  Truss truss;
  truss.uuid = trussUuid;
  truss.name = "Truss";
  truss.position = "legacy-position";
  truss.positionName = "Legacy Position";
  scene.trusses.emplace(trussUuid, truss);
  Support support;
  support.uuid = supportUuid;
  support.name = "Support";
  support.positionName = "Legacy Position";
  scene.supports.emplace(supportUuid, support);
  Fixture unresolved =
      MakeFixture(unresolvedUuid, "Unresolved Fixture", 2, 2, 0.0f);
  unresolved.position = "missing-position";
  scene.fixtures.emplace(unresolvedUuid, unresolved);

  const auto result =
      mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());
  const auto repeated =
      mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());

  assert(result.success);
  assert(result.positions.at(canonicalPosition) == "Canonical Position");
  assert(result.positionReferences.at(fixtureUuid) == canonicalPosition);
  const std::string preparedLegacy = result.positionReferences.at(trussUuid);
  assert(preparedLegacy.size() == 36);
  assert(result.positionReferences.at(supportUuid) == preparedLegacy);
  assert(result.positionReferences.at(unresolvedUuid).empty());
  assert(repeated.positionReferences.at(trussUuid) == preparedLegacy);
  const auto diagnostic =
      result.positionReferenceDiagnostics.find(unresolvedUuid);
  assert(diagnostic != result.positionReferenceDiagnostics.end());
  assert(diagnostic->second.code == MvrExportDiagnosticCode::ReferenceCleared);
  assert(diagnostic->second.severity == MvrExportDiagnosticSeverity::Warning);
  assert(diagnostic->second.impact == MvrExportDiagnosticImpact::DataOmitted);
  assert(diagnostic->second.userVisible);
  assert(scene.positions.contains("legacy-position"));
  assert(scene.fixtures.at(unresolvedUuid).position == "missing-position");
}

// Verifies fatal transform validation retains its established diagnostic model.
void TestFatalTransformPreparation() {
  const std::string fixtureUuid = "99999999-9999-4999-8999-999999999999";
  MvrScene scene;
  Fixture fixture = MakeFixture(fixtureUuid, "Invalid Transform", 1, 1, 0.0f);
  fixture.transform.o[0] = std::numeric_limits<float>::quiet_NaN();
  scene.fixtures.emplace(fixtureUuid, fixture);

  const auto result =
      mvr_export_preparation::Prepare(scene, CanonicalMvrExportOptions());
  const MvrExportDiagnostic *diagnostic = FindDiagnostic(
      result.diagnostics, MvrExportDiagnosticCode::TransformInvalid);

  assert(!result.success);
  assert(diagnostic != nullptr);
  assert(diagnostic->severity == MvrExportDiagnosticSeverity::Error);
  assert(diagnostic->impact == MvrExportDiagnosticImpact::ExportFailed);
  assert(diagnostic->userVisible);
  assert(diagnostic->objectIdentity == fixtureUuid);
  assert(std::isnan(scene.fixtures.at(fixtureUuid).transform.o[0]));
}

// Verifies preparation is stable across repetition and map insertion order.
void TestDeterminismAcrossInsertionOrder() {
  const std::string firstUuid = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
  const std::string secondUuid = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
  MvrScene forward;
  forward.fixtures.emplace(firstUuid, MakeFixture(firstUuid, "A", 0, 0, 4.0f));
  forward.fixtures.emplace(secondUuid,
                           MakeFixture(secondUuid, "B", 0, 0, 2.0f));
  MvrScene reverse;
  reverse.fixtures.emplace(secondUuid,
                           MakeFixture(secondUuid, "B", 0, 0, 2.0f));
  reverse.fixtures.emplace(firstUuid, MakeFixture(firstUuid, "A", 0, 0, 4.0f));
  const std::string firstPosition = "10101010-1010-4010-8010-101010101010";
  const std::string secondPosition = "legacy-position-deterministic";
  forward.positions.emplace(firstPosition, "Front");
  forward.positions.emplace(secondPosition, "Back");
  reverse.positions.emplace(secondPosition, "Back");
  reverse.positions.emplace(firstPosition, "Front");
  Layer firstLayer{firstPosition, "Fixtures", "#112233"};
  Layer secondLayer{"legacy-layer-two", "Rigging", "#445566"};
  forward.layers.emplace(firstLayer.uuid, firstLayer);
  forward.layers.emplace(secondLayer.uuid, secondLayer);
  reverse.layers.emplace(secondLayer.uuid, secondLayer);
  reverse.layers.emplace(firstLayer.uuid, firstLayer);

  const auto first =
      mvr_export_preparation::Prepare(forward, CanonicalMvrExportOptions());
  const auto repeated =
      mvr_export_preparation::Prepare(forward, CanonicalMvrExportOptions());
  const auto reordered =
      mvr_export_preparation::Prepare(reverse, CanonicalMvrExportOptions());

  assert(first.success && repeated.success && reordered.success);
  assert(first.objectIds == repeated.objectIds);
  assert(first.fixtureUnitNumbers == repeated.fixtureUnitNumbers);
  assert(first.objectIds == reordered.objectIds);
  assert(first.fixtureUnitNumbers == reordered.fixtureUnitNumbers);
  assert(first.positions == repeated.positions);
  assert(first.positions == reordered.positions);
  assert(first.layerUuids == repeated.layerUuids);
  assert(first.layerUuids == reordered.layerUuids);
}

} // namespace

// Runs focused MVR export preparation characterization.
int main() {
  TestPreparedValuesAndSourceImmutability();
  TestPositionPreparation();
  TestFatalTransformPreparation();
  TestDeterminismAcrossInsertionOrder();
  return 0;
}
