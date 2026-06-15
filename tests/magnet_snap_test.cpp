#include "magnet_snap.h"

#include "matrixutils.h"
#include "scene_grouping.h"

#include <cassert>
#include <cmath>

namespace {

// Builds a translated identity matrix.
Matrix Translated(float x, float y, float z) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o = {x, y, z};
  return matrix;
}

// Adds a truss with common test dimensions.
void AddTruss(MvrScene &scene, const std::string &uuid, float x) {
  Truss truss;
  truss.uuid = uuid;
  truss.layer = "No Layer";
  truss.lengthMm = 3000.0f;
  truss.widthMm = 300.0f;
  truss.heightMm = 300.0f;
  truss.position = "LX1";
  truss.positionName = "LX1";
  truss.transform = Translated(x, 0.0f, 0.0f);
  scene.trusses[uuid] = truss;
}

} // namespace

// Verifies Magnet snap candidates, metadata preservation, and committed grouping.
int main() {
  MvrScene scene;
  AddTruss(scene, "target", 0.0f);
  AddTruss(scene, "source", 3250.0f);

  auto snap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source"});
  assert(snap);
  assert(snap->kind == magnet_snap::SnapKind::TrussToTruss);
  assert(snap->needsGrouping);
  assert(std::fabs(snap->translationDeltaMm[0] + 250.0f) < 0.001f);

  scene.trusses["source"].transform.o[0] = 4000.0f;
  assert(!magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source"}));

  scene.trusses["source"].transform.o[0] = 3250.0f;
  snap = magnet_snap::FindSnap(scene,
                               {magnet_snap::ObjectType::Truss, "source"});
  assert(snap);
  const std::string hang = scene.trusses["source"].positionName;
  assert(magnet_snap::ApplySnapTransform(scene, *snap));
  assert(scene.trusses["source"].positionName == hang);
  assert(magnet_snap::ApplyCommittedSnapGrouping(scene, *snap));
  assert(scene.groupObjects.size() == 1);
  const std::string groupUuid = scene.trusses["target"].parentGroupUuid;
  assert(!groupUuid.empty());

  AddTruss(scene, "source-2", 6250.0f);
  scene.trusses["source-2"].transform.o[0] = 3250.0f;
  auto snap2 = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source-2"});
  assert(snap2);
  assert(magnet_snap::ApplyCommittedSnapGrouping(scene, *snap2));
  assert(scene.trusses["source-2"].parentGroupUuid == groupUuid);
  assert(scene.groupObjects.size() == 1);

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.transform = Translated(1510.0f, 160.0f, 0.0f);
  scene.fixtures[fixture.uuid] = fixture;
  auto fixtureSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Fixture, fixture.uuid});
  assert(fixtureSnap);
  assert(fixtureSnap->kind == magnet_snap::SnapKind::FixtureToTruss);
  assert(fixtureSnap->needsGrouping);
  assert(std::fabs(fixtureSnap->translationDeltaMm[1] + 10.0f) < 0.001f);
  assert(std::fabs(fixtureSnap->translationDeltaMm[2]) < 0.001f);

  Fixture topEdgeFixture;
  topEdgeFixture.uuid = "top-edge-fixture";
  topEdgeFixture.transform = Translated(1510.0f, 160.0f, 300.0f);
  scene.fixtures[topEdgeFixture.uuid] = topEdgeFixture;
  auto topEdgeFixtureSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Fixture, topEdgeFixture.uuid});
  assert(topEdgeFixtureSnap);
  assert(topEdgeFixtureSnap->kind == magnet_snap::SnapKind::FixtureToTruss);
  assert(std::fabs(topEdgeFixtureSnap->translationDeltaMm[1] + 10.0f) < 0.001f);
  assert(std::fabs(topEdgeFixtureSnap->translationDeltaMm[2]) < 0.001f);

  assert(magnet_snap::ApplyCommittedSnapGrouping(scene, *fixtureSnap));
  assert(scene.fixtures[fixture.uuid].parentGroupUuid == groupUuid);

  SceneObject object;
  object.uuid = "object";
  object.transform = Translated(0.0f, 650.0f, 0.0f);
  scene.sceneObjects[object.uuid] = object;
  auto objectSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::SceneObject, object.uuid});
  assert(objectSnap);
  assert(objectSnap->kind == magnet_snap::SnapKind::SceneObjectToObject);

  magnet_snap::SnapResult groupedTrussMove;
  groupedTrussMove.snapped = true;
  groupedTrussMove.sourceType = magnet_snap::ObjectType::Truss;
  groupedTrussMove.sourceUuid = "target";
  groupedTrussMove.translationDeltaMm = {0.0f, 0.0f, 100.0f};
  const float fixtureZBeforeGroupMove =
      scene.fixtures[fixture.uuid].transform.o[2];
  const float trussZBeforeGroupMove = scene.trusses["target"].transform.o[2];
  assert(magnet_snap::ApplySnapTransform(scene, groupedTrussMove));
  assert(std::fabs(scene.fixtures[fixture.uuid].transform.o[2] -
                   fixtureZBeforeGroupMove - 100.0f) < 0.001f);
  assert(std::fabs(scene.trusses["target"].transform.o[2] -
                   trussZBeforeGroupMove - 100.0f) < 0.001f);

  assert(magnet_snap::DetachSnapSourceFromGroup(scene, *fixtureSnap));
  assert(scene.fixtures[fixture.uuid].parentGroupUuid.empty());
  assert(scene.trusses["target"].parentGroupUuid == groupUuid);

  return 0;
}
