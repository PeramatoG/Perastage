#include "scene_grouping.h"

#include "matrixutils.h"

#include <cassert>
#include <cmath>

namespace {

// Compares matrix translations with a small tolerance.
bool NearlyEqual(float a, float b) {
  return std::fabs(a - b) < 0.001f;
}

// Builds a translated identity matrix.
Matrix Translated(float x, float y, float z) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o = {x, y, z};
  return matrix;
}

} // namespace

// Verifies that grouping preserves world transforms and hang positions.
int main() {
  MvrScene scene;

  Fixture fixture;
  fixture.uuid = "fixture-a";
  fixture.layer = "No Layer";
  fixture.position = "lx1-position";
  fixture.positionName = "LX1";
  fixture.transform = Translated(1000.0f, 0.0f, 0.0f);
  scene.fixtures[fixture.uuid] = fixture;

  Truss truss;
  truss.uuid = "truss-a";
  truss.layer = "No Layer";
  truss.position = "lx1-position";
  truss.positionName = "LX1";
  truss.transform = Translated(3000.0f, 0.0f, 0.0f);
  scene.trusses[truss.uuid] = truss;

  scene_grouping::ObjectSelection selection;
  selection.fixtures = {fixture.uuid};
  selection.trusses = {truss.uuid};

  const scene_grouping::OperationResult groupResult =
      scene_grouping::GroupSelection(scene, selection);
  assert(groupResult.changed);
  assert(scene.groupObjects.size() == 1);
  assert(scene.fixtures[fixture.uuid].parentGroupUuid == groupResult.groupUuid);
  assert(scene.trusses[truss.uuid].parentGroupUuid == groupResult.groupUuid);
  assert(scene.fixtures[fixture.uuid].positionName == "LX1");
  assert(scene.trusses[truss.uuid].positionName == "LX1");
  assert(NearlyEqual(scene.fixtures[fixture.uuid].transform.o[0], 1000.0f));
  assert(NearlyEqual(scene.trusses[truss.uuid].transform.o[0], 3000.0f));

  const auto expanded =
      scene_grouping::ExpandSelectionForGroupHighlights(scene, selection);
  assert(expanded.size() == 2);

  const scene_grouping::OperationResult ungroupResult =
      scene_grouping::UngroupSelection(scene, selection);
  assert(ungroupResult.changed);
  assert(scene.groupObjects.empty());
  assert(scene.fixtures[fixture.uuid].parentGroupUuid.empty());
  assert(scene.trusses[truss.uuid].parentGroupUuid.empty());
  assert(scene.fixtures[fixture.uuid].positionName == "LX1");
  assert(scene.trusses[truss.uuid].positionName == "LX1");
  assert(NearlyEqual(scene.fixtures[fixture.uuid].transform.o[0], 1000.0f));
  assert(NearlyEqual(scene.trusses[truss.uuid].transform.o[0], 3000.0f));


  const scene_grouping::OperationResult regroupResult =
      scene_grouping::GroupSelection(scene, selection);
  assert(regroupResult.changed);

  Support support;
  support.uuid = "support-a";
  support.layer = "No Layer";
  support.position = "lx1-position";
  support.positionName = "LX1";
  support.transform = Translated(5000.0f, 0.0f, 0.0f);
  scene.supports[support.uuid] = support;

  scene_grouping::ObjectSelection nestedSelection;
  nestedSelection.fixtures = {fixture.uuid};
  nestedSelection.trusses = {truss.uuid};
  nestedSelection.supports = {support.uuid};
  const scene_grouping::OperationResult nestedGroupResult =
      scene_grouping::GroupSelection(scene, nestedSelection);
  assert(nestedGroupResult.changed);
  assert(scene.groupObjects.size() == 2);

  scene_grouping::ObjectSelection childSelection;
  childSelection.fixtures = {fixture.uuid};
  assert(scene_grouping::BuildTransformTargets(scene, childSelection).size() == 1);
  const auto nestedExpanded =
      scene_grouping::ExpandSelectionForGroupHighlights(scene, childSelection);
  assert(nestedExpanded.size() == 1);

  scene_grouping::TranslateSelection(scene, childSelection,
                                     {1000.0f, 2000.0f, 0.0f});
  assert(NearlyEqual(scene.fixtures[fixture.uuid].transform.o[0], 2000.0f));
  assert(NearlyEqual(scene.trusses[truss.uuid].transform.o[0], 3000.0f));
  assert(NearlyEqual(scene.supports[support.uuid].transform.o[0], 5000.0f));
  assert(NearlyEqual(scene.fixtures[fixture.uuid].transform.o[1], 2000.0f));
  assert(NearlyEqual(scene.trusses[truss.uuid].transform.o[1], 0.0f));
  assert(NearlyEqual(scene.supports[support.uuid].transform.o[1], 0.0f));

  MvrScene groupedDragScene;
  Fixture groupedFixture;
  groupedFixture.uuid = "grouped-fixture";
  groupedFixture.layer = "No Layer";
  groupedFixture.transform = Translated(1000.0f, 0.0f, 0.0f);
  groupedDragScene.fixtures[groupedFixture.uuid] = groupedFixture;
  Truss groupedTruss;
  groupedTruss.uuid = "grouped-truss";
  groupedTruss.layer = "No Layer";
  groupedTruss.transform = Translated(3000.0f, 0.0f, 0.0f);
  groupedDragScene.trusses[groupedTruss.uuid] = groupedTruss;
  scene_grouping::ObjectSelection groupedDragSelection;
  groupedDragSelection.fixtures = {groupedFixture.uuid};
  groupedDragSelection.trusses = {groupedTruss.uuid};
  assert(scene_grouping::GroupSelection(groupedDragScene, groupedDragSelection)
             .changed);
  assert(scene_grouping::BuildTransformTargets(groupedDragScene,
                                               groupedDragSelection)
             .size() == 1);
  scene_grouping::TranslateSelection(groupedDragScene, groupedDragSelection,
                                     {100.0f, 0.0f, 0.0f});
  assert(NearlyEqual(groupedDragScene.fixtures[groupedFixture.uuid]
                         .transform.o[0],
                     1100.0f));
  assert(NearlyEqual(groupedDragScene.trusses[groupedTruss.uuid].transform.o[0],
                     3100.0f));

  const scene_grouping::OperationResult fixtureDetachResult =
      scene_grouping::RemoveSelectionFromGroup(scene, childSelection);
  assert(fixtureDetachResult.changed);
  assert(scene.groupObjects.size() == 2);
  assert(scene.fixtures[fixture.uuid].parentGroupUuid.empty());
  assert(!scene.trusses[truss.uuid].parentGroupUuid.empty());
  assert(NearlyEqual(scene.fixtures[fixture.uuid].transform.o[0], 2000.0f));
  assert(NearlyEqual(scene.trusses[truss.uuid].transform.o[0], 3000.0f));
  assert(NearlyEqual(scene.supports[support.uuid].transform.o[0], 5000.0f));

  return 0;
}
