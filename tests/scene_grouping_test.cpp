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

  return 0;
}
