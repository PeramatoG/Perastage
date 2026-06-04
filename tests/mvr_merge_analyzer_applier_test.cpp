#include "mvr_merge_analyzer.h"
#include "mvr_merge_applier.h"

#include <cassert>
#include <string>

// Builds a fixture with the requested UUID and position reference.
static Fixture MakeFixture(const std::string &uuid, const std::string &position) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.position = position;
  return fixture;
}

// Verifies merge analysis resolves collisions before applying imported scene data.
int main() {
  MvrScene target;
  target.positions["position-a"] = "Existing position";
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");

  MvrScene imported;
  imported.positions["position-a"] = "Imported position";
  imported.fixtures["fixture-b"] = MakeFixture("fixture-b", "position-a");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  assert(analysis.uuidCollisionsResolved == 1);
  assert(analysis.uuidMap.at("position-a") != "position-a");

  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, analysis);
  assert(result.uuidCollisionsResolved == 1);
  assert(result.fixturesAdded == 1);
  assert(target.fixtures.count("fixture-a") == 1);
  assert(target.fixtures.count("fixture-b") == 1);
  assert(target.fixtures.at("fixture-b").position ==
         analysis.uuidMap.at("position-a"));
  assert(target.positions.count(analysis.uuidMap.at("position-a")) == 1);
  return 0;
}
