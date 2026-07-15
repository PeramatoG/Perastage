#include "configservices.h"

#include <cassert>

// Verifies stale selection UUID detection is bounded and type-specific.
int main() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture-ok";
  scene.fixtures[fixture.uuid] = fixture;
  Truss truss;
  truss.uuid = "truss-ok";
  scene.trusses[truss.uuid] = truss;

  SelectionState selection;
  selection.SetSelectedFixtures({"fixture-ok", "fixture-missing-a", "fixture-missing-b"});
  selection.SetSelectedTrusses({"truss-ok"});
  selection.SetSelectedSupports({"support-missing"});

  const auto issues = FindStaleSelectionUuids(scene, selection, 1);
  assert(issues.size() == 2);
  assert(issues[0].entityType == "fixtures");
  assert(issues[0].missingCount == 2);
  assert(issues[0].sampleUuids.size() == 1);
  assert(issues[1].entityType == "supports");
  assert(issues[1].missingCount == 1);
  return 0;
}
