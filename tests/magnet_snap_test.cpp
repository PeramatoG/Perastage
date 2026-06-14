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
  assert(snap->needsTrussGrouping);
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
  assert(magnet_snap::ApplyCommittedTrussGrouping(scene, *snap));
  assert(scene.groupObjects.size() == 1);
  const std::string groupUuid = scene.trusses["target"].parentGroupUuid;
  assert(!groupUuid.empty());

  AddTruss(scene, "source-2", 6250.0f);
  scene.trusses["source-2"].transform.o[0] = 3250.0f;
  auto snap2 = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source-2"});
  assert(snap2);
  assert(magnet_snap::ApplyCommittedTrussGrouping(scene, *snap2));
  assert(scene.trusses["source-2"].parentGroupUuid == groupUuid);
  assert(scene.groupObjects.size() == 1);

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.transform = Translated(0.0f, 0.0f, 140.0f);
  scene.fixtures[fixture.uuid] = fixture;
  auto fixtureSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Fixture, fixture.uuid});
  assert(fixtureSnap);
  assert(fixtureSnap->kind == magnet_snap::SnapKind::FixtureToTruss);
  assert(!fixtureSnap->needsTrussGrouping);

  SceneObject object;
  object.uuid = "object";
  object.transform = Translated(0.0f, 650.0f, 0.0f);
  scene.sceneObjects[object.uuid] = object;
  auto objectSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::SceneObject, object.uuid});
  assert(objectSnap);
  assert(objectSnap->kind == magnet_snap::SnapKind::SceneObjectToObject);

  return 0;
}
