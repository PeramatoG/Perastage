#include "mvr_merge_analyzer.h"
#include "mvr_merge_applier.h"

#include <cassert>
#include <string>

// Builds a fixture with the requested UUID and position reference.
static Fixture MakeFixture(const std::string &uuid,
                           const std::string &position) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.position = position;
  return fixture;
}

// Builds a support with the requested UUID, position reference, and motor
// fixture reference.
static Support MakeSupport(const std::string &uuid, const std::string &position,
                           const std::string &motorFixtureUuid) {
  Support support;
  support.uuid = uuid;
  support.position = position;
  support.motorFixtureUuid = motorFixtureUuid;
  return support;
}

// Builds a group object that references one child UUID.
static GroupObject MakeGroup(const std::string &uuid, MvrNodeType childType,
                             const std::string &childUuid) {
  GroupObject group;
  group.uuid = uuid;
  group.children.push_back(GroupObjectChildRef{childType, childUuid});
  return group;
}

// Verifies fixture UUID collisions use deterministic replacement UUIDs.
static void VerifyFixtureUuidCollisionUsesStableReplacement() {
  MvrScene target;
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");

  MvrScene imported;
  imported.fixtures["fixture-a"] = MakeFixture("fixture-a", "");

  const mvr::MvrMergeAnalysis firstAnalysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const mvr::MvrMergeAnalysis secondAnalysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  assert(firstAnalysis.uuidCollisionsDetected == 1);
  assert(firstAnalysis.uuidCollisionsResolved == 1);
  assert(firstAnalysis.uuidMap.at("fixture-a") != "fixture-a");
  assert(firstAnalysis.uuidMap.at("fixture-a") ==
         secondAnalysis.uuidMap.at("fixture-a"));
  assert(firstAnalysis.fixtureUuidRemap.at("fixture-a") ==
         firstAnalysis.uuidMap.at("fixture-a"));

  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, firstAnalysis);
  assert(result.uuidCollisionsResolved == 1);
  assert(result.fixturesAdded == 1);
  assert(result.fixtureUuidRemap.at("fixture-a") ==
         firstAnalysis.uuidMap.at("fixture-a"));
  assert(target.fixtures.count("fixture-a") == 1);
  assert(target.fixtures.count(firstAnalysis.uuidMap.at("fixture-a")) == 1);
}

// Verifies group child references follow UUID remaps for colliding imported
// children.
static void VerifyGroupChildRemapping() {
  MvrScene target;
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "");

  MvrScene imported;
  imported.fixtures["fixture-a"] = MakeFixture("fixture-a", "");
  imported.groupObjects["group-a"] =
      MakeGroup("group-a", MvrNodeType::Fixture, "fixture-a");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const std::string remappedFixtureUuid = analysis.uuidMap.at("fixture-a");
  mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(target.groupObjects.count("group-a") == 1);
  assert(target.groupObjects.at("group-a").children.size() == 1);
  assert(target.groupObjects.at("group-a").children.front().uuid ==
         remappedFixtureUuid);
}

// Verifies support position and linked motor fixture references follow remapped
// UUIDs.
static void VerifySupportPositionReferenceRemapping() {
  MvrScene target;
  target.positions["position-a"] = "Existing position";
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");

  MvrScene imported;
  imported.positions["position-a"] = "Imported position";
  imported.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");
  imported.supports["support-a"] =
      MakeSupport("support-a", "position-a", "fixture-a");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const std::string remappedPositionUuid = analysis.uuidMap.at("position-a");
  const std::string remappedFixtureUuid = analysis.uuidMap.at("fixture-a");
  mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(target.positions.count(remappedPositionUuid) == 1);
  assert(target.supports.count("support-a") == 1);
  assert(target.supports.at("support-a").position == remappedPositionUuid);
  assert(target.supports.at("support-a").motorFixtureUuid ==
         remappedFixtureUuid);
}

// Verifies merge analysis resolves collisions before applying imported scene
// data.
int main() {
  VerifyFixtureUuidCollisionUsesStableReplacement();
  VerifyGroupChildRemapping();
  VerifySupportPositionReferenceRemapping();
  return 0;
}
