#include "scene_node_operations.h"

#include "matrixutils.h"

#include <cassert>
#include <cmath>
#include <utility>

namespace {

// Builds a translated and rotated matrix for node-operation fixtures.
Matrix Transform(float x, float y, float z, float yaw) {
  Matrix matrix = MatrixUtils::EulerToMatrix(yaw, 7.0f, 13.0f);
  matrix.o = {x, y, z};
  return matrix;
}

// Compares all matrix components using the transform contract tolerance.
bool MatrixEqual(const Matrix &lhs, const Matrix &rhs) {
  for (const auto &pair : {std::pair{&lhs.u, &rhs.u},
                           std::pair{&lhs.v, &rhs.v},
                           std::pair{&lhs.w, &rhs.w},
                           std::pair{&lhs.o, &rhs.o}}) {
    for (size_t index = 0; index < 3; ++index) {
      if (std::fabs((*pair.first)[index] - (*pair.second)[index]) > 0.001f)
        return false;
    }
  }
  return true;
}

// Creates one canonical group containing all supported leaf node types.
MvrScene GroupedScene() {
  MvrScene scene;
  GroupObject group;
  group.uuid = "group";
  group.transform = Transform(100.0f, 200.0f, 300.0f, 25.0f);
  group.localTransform = group.transform;
  scene.groupObjects[group.uuid] = group;

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.instanceName = "Motor";
  fixture.typeName = "Motor Type";
  fixture.function = "Lighting";
  fixture.weightKg = 12.0f;
  fixture.parentGroupUuid = group.uuid;
  fixture.localTransform = Transform(10.0f, 20.0f, 30.0f, 5.0f);
  fixture.hasLocalTransform = true;
  fixture.transform = MatrixUtils::Multiply(group.transform,
                                            fixture.localTransform);
  scene.fixtures[fixture.uuid] = fixture;
  scene.groupObjects[group.uuid].children.push_back(
      {MvrNodeType::Fixture, fixture.uuid});

  Truss truss;
  truss.uuid = "truss";
  truss.parentGroupUuid = group.uuid;
  truss.localTransform = Transform(40.0f, 50.0f, 60.0f, 15.0f);
  truss.hasLocalTransform = true;
  truss.transform = MatrixUtils::Multiply(group.transform,
                                          truss.localTransform);
  scene.trusses[truss.uuid] = truss;
  scene.groupObjects[group.uuid].children.push_back(
      {MvrNodeType::Truss, truss.uuid});
  return scene;
}

} // namespace

// Verifies exact table transforms, conversion, and hierarchy-safe deletion.
int main() {
  MvrScene scene = GroupedScene();
  const Matrix unchangedGroup = scene.groupObjects["group"].transform;
  const Matrix unchangedFixture = scene.fixtures["fixture"].transform;
  const Matrix requestedTruss = Transform(900.0f, 800.0f, 700.0f, 42.0f);
  assert(scene_node_operations::ApplyExactWorldTransform(
      scene, MvrNodeType::Truss, "truss", requestedTruss));
  assert(MatrixEqual(scene.trusses["truss"].transform, requestedTruss));
  assert(MatrixEqual(scene.groupObjects["group"].transform, unchangedGroup));
  assert(MatrixEqual(scene.fixtures["fixture"].transform, unchangedFixture));
  assert(scene.trusses["truss"].parentGroupUuid == "group");
  assert(MatrixEqual(MatrixUtils::Multiply(
                         scene.groupObjects["group"].transform,
                         scene.trusses["truss"].localTransform),
                     requestedTruss));

  const Matrix fixtureWorld = scene.fixtures["fixture"].transform;
  const Matrix fixtureLocal = scene.fixtures["fixture"].localTransform;
  const auto conversion =
      scene_node_operations::ConvertFixtureToSupport(scene, "fixture");
  assert(conversion.changed && conversion.uuid == "fixture");
  assert(!scene.fixtures.contains("fixture"));
  assert(scene.supports.contains("fixture"));
  assert(scene.supports["fixture"].motorFixtureUuid.empty());
  assert(scene.supports["fixture"].parentGroupUuid == "group");
  assert(MatrixEqual(scene.supports["fixture"].transform, fixtureWorld));
  assert(MatrixEqual(scene.supports["fixture"].localTransform, fixtureLocal));
  assert(scene.groupObjects["group"].children.front().type ==
         MvrNodeType::Support);

  auto removal = scene_node_operations::RemoveNodes(
      scene, {{MvrNodeType::Support, "fixture"},
              {MvrNodeType::Support, "fixture"}});
  assert(removal.changed);
  assert(!scene.supports.contains("fixture"));
  assert(scene.groupObjects.contains("group"));
  assert(scene.groupObjects["group"].children.size() == 1);

  removal = scene_node_operations::RemoveNodes(
      scene, {{MvrNodeType::Truss, "truss"}});
  assert(removal.changed);
  assert(!scene.trusses.contains("truss"));
  assert(!scene.groupObjects.contains("group"));
  assert(removal.removedEmptyGroups.size() == 1);

  MvrScene linkedFixtureScene;
  Fixture linkedFixture;
  linkedFixture.uuid = "linked-fixture";
  linkedFixtureScene.fixtures[linkedFixture.uuid] = linkedFixture;
  Support linkedSupport;
  linkedSupport.uuid = "linked-support";
  linkedSupport.motorFixtureUuid = linkedFixture.uuid;
  linkedFixtureScene.supports[linkedSupport.uuid] = linkedSupport;
  removal = scene_node_operations::RemoveNodes(
      linkedFixtureScene, {{MvrNodeType::Fixture, linkedFixture.uuid}});
  assert(removal.changed);
  assert(linkedFixtureScene.supports[linkedSupport.uuid]
             .motorFixtureUuid.empty());

  MvrScene nested;
  GroupObject root;
  root.uuid = "root";
  root.children.push_back({MvrNodeType::GroupObject, "child"});
  GroupObject child;
  child.uuid = "child";
  child.parentGroupUuid = "root";
  SceneObject object;
  object.uuid = "object";
  object.parentGroupUuid = "child";
  child.children.push_back({MvrNodeType::SceneObject, object.uuid});
  nested.groupObjects[root.uuid] = root;
  nested.groupObjects[child.uuid] = child;
  nested.sceneObjects[object.uuid] = object;
  removal = scene_node_operations::RemoveNodes(
      nested, {{MvrNodeType::GroupObject, "root"}});
  assert(removal.changed);
  assert(nested.groupObjects.empty());
  assert(nested.sceneObjects.empty());
  return 0;
}
