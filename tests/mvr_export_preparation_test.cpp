#include "mvr_export_preparation.h"

#include <cassert>

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
  assert(!result.diagnostics.empty());
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
}

} // namespace

// Runs focused MVR export preparation characterization.
int main() {
  TestPreparedValuesAndSourceImmutability();
  TestDeterminismAcrossInsertionOrder();
  return 0;
}
