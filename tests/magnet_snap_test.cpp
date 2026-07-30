#include "magnet_snap.h"

#include "matrixutils.h"
#include "scene_grouping.h"
#include "truss_attachment_candidates.h"

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

// Verifies Magnet snap candidates, metadata preservation, and committed
// grouping.
int main() {
  using truss_attachment::CandidateKind;
  const auto longitudinal = truss_attachment::BuildInferredCandidates(
      {3000.0f, 400.0f, 400.0f}, Translated(100.0f, 200.0f, 300.0f), "long");
  assert(longitudinal.size() == 2);
  assert(longitudinal[0].kind == CandidateKind::InferredLongitudinalEnd);
  assert(longitudinal[0].localTransform.o[0] == 0.0f);
  assert(longitudinal[1].localTransform.o[0] == 3000.0f);
  assert(longitudinal[0].worldTransform.o[0] == 100.0f);
  assert(longitudinal[1].worldTransform.o[0] == 3100.0f);
  assert(truss_attachment::ClassifyLongitudinalAxis({3000, 400, 400}) == 0);
  assert(truss_attachment::ClassifyLongitudinalAxis({400, 3000, 400}) == 1);
  assert(truss_attachment::ClassifyLongitudinalAxis({400, 400, 3000}) == 2);
  assert(!truss_attachment::ClassifyLongitudinalAxis({800, 400, 400}));
  assert(truss_attachment::ClassifyLongitudinalAxis({801, 400, 400}) == 0);

  const auto ambiguous = truss_attachment::BuildInferredCandidates(
      {400.0f, 400.0f, 400.0f}, MatrixUtils::Identity(), "cube");
  assert(ambiguous.size() == 6);
  assert(ambiguous.front().stableId == "face-axis-0-negative");
  assert(ambiguous.back().stableId == "face-axis-2-positive");
  for (const auto &candidate : ambiguous)
    assert(candidate.kind == CandidateKind::InferredFaceCenter);
  assert(truss_attachment::BuildInferredCandidates({0.0f, 400.0f, 400.0f},
                                                   MatrixUtils::Identity())
             .size() == 6);

  const std::string magnetXml =
      "<GDTF><FixtureType><Geometries>"
      "<Geometry Name='Root' Position='{1,0,0,1}{0,1,0,2}{0,0,1,3}{0,0,0,1}'>"
      "<Magnet Name='A' Model='Connector' "
      "Position='{1,0,0,0.5}{0,1,0,0}{0,0,1,0}{0,0,0,1}'/>"
      "<Geometry><Magnet Name='B'/></Geometry>"
      "<Magnet Name='Bad' Position='invalid'/></Geometry>"
      "</Geometries></FixtureType></GDTF>";
  const auto explicitMagnets = truss_attachment::ReadExplicitGdtfMagnets(
      magnetXml, Translated(100.0f, 0.0f, 0.0f));
  assert(explicitMagnets.candidates.size() == 2);
  assert(explicitMagnets.diagnostics.size() == 1);
  assert(explicitMagnets.candidates[0].name == "A");
  assert(explicitMagnets.candidates[0].model == "Connector");
  assert(explicitMagnets.candidates[1].model.empty());
  assert(explicitMagnets.candidates[0].localTransform.o[0] == 1500.0f);
  assert(explicitMagnets.candidates[0].localTransform.o[1] == 2000.0f);
  assert(explicitMagnets.candidates[0].worldTransform.o[0] == 1600.0f);
  assert(truss_attachment::ReadExplicitGdtfMagnets(
             "<GDTF><FixtureType><Geometries/></FixtureType></GDTF>",
             MatrixUtils::Identity())
             .candidates.empty());

  MvrScene scene;
  AddTruss(scene, "target", 0.0f);
  AddTruss(scene, "source", 3250.0f);

  auto snap =
      magnet_snap::FindSnap(scene, {magnet_snap::ObjectType::Truss, "source"});
  assert(snap);
  assert(snap->kind == magnet_snap::SnapKind::TrussToTruss);
  assert(snap->needsGrouping);
  assert(std::fabs(snap->translationDeltaMm[0] + 250.0f) < 0.001f);

  scene.trusses["source"].transform.o[0] = 4000.0f;
  assert(!magnet_snap::FindSnap(scene,
                                {magnet_snap::ObjectType::Truss, "source"}));

  MvrScene sideSurfaceScene;
  AddTruss(sideSurfaceScene, "target", 0.0f);
  AddTruss(sideSurfaceScene, "source", 0.0f);
  sideSurfaceScene.trusses["source"].transform.o[1] = 550.0f;
  magnet_snap::SnapSettings topSideSettings;
  topSideSettings.axisWeights[2] = 0.0f;
  auto sideSurfaceSnap = magnet_snap::FindSnap(
      sideSurfaceScene, {magnet_snap::ObjectType::Truss, "source"},
      topSideSettings);
  assert(!sideSurfaceSnap);

  scene.trusses["source"].transform.o[0] = 3250.0f;
  snap =
      magnet_snap::FindSnap(scene, {magnet_snap::ObjectType::Truss, "source"});
  assert(snap);
  const std::string hang = scene.trusses["source"].positionName;
  assert(magnet_snap::ApplySnapTransform(scene, *snap));
  assert(scene.trusses["source"].positionName == hang);
  assert(magnet_snap::ApplyCommittedSnapGrouping(scene, *snap));
  assert(scene.groupObjects.size() == 1);
  const std::string groupUuid = scene.trusses["target"].parentGroupUuid;
  assert(!groupUuid.empty());

  AddTruss(scene, "loose", 6250.0f);
  auto groupSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::TrussGroup, groupUuid});
  assert(groupSnap);
  assert(groupSnap->kind == magnet_snap::SnapKind::TrussToTruss);
  assert(groupSnap->sourceUuid == groupUuid);
  assert(groupSnap->targetUuid == "loose");
  assert(std::fabs(groupSnap->translationDeltaMm[0] - 250.0f) < 0.001f);
  scene.trusses.erase("loose");

  AddTruss(scene, "source-2", 6250.0f);
  auto snap2 = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "source-2"});
  assert(snap2);
  assert(snap2->targetUuid == groupUuid);
  assert(magnet_snap::ApplyCommittedSnapGrouping(scene, *snap2));
  assert(scene.trusses["source-2"].parentGroupUuid == groupUuid);
  assert(scene.groupObjects.size() == 1);

  AddTruss(scene, "interior-candidate", 3250.0f);
  assert(!magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "interior-candidate"}));
  scene.trusses.erase("interior-candidate");

  magnet_snap::SnapSettings topViewSettings;
  topViewSettings.axisWeights[2] = 0.0f;
  AddTruss(scene, "top-view-source", 9250.0f);
  scene.trusses["top-view-source"].transform.o[2] = 1000.0f;
  auto topViewSnap = magnet_snap::FindSnap(
      scene, {magnet_snap::ObjectType::Truss, "top-view-source"},
      topViewSettings);
  assert(topViewSnap);
  assert(std::fabs(topViewSnap->translationDeltaMm[2]) > 0.001f);
  scene.trusses.erase("top-view-source");

  MvrScene elevatedFixtureScene;
  AddTruss(elevatedFixtureScene, "elevated-truss", 0.0f);
  elevatedFixtureScene.trusses["elevated-truss"].transform.o[2] = 10000.0f;
  Fixture elevatedFixture;
  elevatedFixture.uuid = "elevated-fixture";
  elevatedFixture.transform = Translated(1510.0f, 160.0f, 3000.0f);
  elevatedFixtureScene.fixtures[elevatedFixture.uuid] = elevatedFixture;
  auto elevatedFixtureSnap = magnet_snap::FindSnap(
      elevatedFixtureScene,
      {magnet_snap::ObjectType::Fixture, elevatedFixture.uuid},
      topViewSettings);
  assert(elevatedFixtureSnap);
  assert(elevatedFixtureSnap->targetUuid == "elevated-truss");
  assert(std::fabs(elevatedFixtureSnap->translationDeltaMm[1] + 10.0f) <
         0.001f);
  assert(std::fabs(elevatedFixtureSnap->translationDeltaMm[2] - 7000.0f) <
         0.001f);
  assert(magnet_snap::ApplySnapTransform(elevatedFixtureScene,
                                         *elevatedFixtureSnap));
  assert(
      std::fabs(
          elevatedFixtureScene.fixtures[elevatedFixture.uuid].transform.o[2] -
          10000.0f) < 0.001f);

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.layer = "MAC500";
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
  assert(scene.fixtures[fixture.uuid].layer ==
         scene.groupObjects[groupUuid].layer);
  assert(scene.trusses["target"].layer == scene.groupObjects[groupUuid].layer);

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
