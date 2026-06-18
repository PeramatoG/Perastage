#include "continuous_placement_scene.h"
#include "mvrscene.h"

#include <cassert>

// Verifies cloning and removal for every supported continuous placement type.
int main() {
  MvrScene scene;

  Fixture fixture;
  fixture.uuid = "fixture-1";
  fixture.fixtureId = 7;
  fixture.transform.o = {1000.0f, 2000.0f, 3000.0f};
  scene.fixtures[fixture.uuid] = fixture;

  Truss truss;
  truss.uuid = "truss-1";
  truss.transform.o = {4000.0f, 5000.0f, 6000.0f};
  scene.trusses[truss.uuid] = truss;

  SceneObject object;
  object.uuid = "object-1";
  object.transform.o = {7000.0f, 8000.0f, 9000.0f};
  scene.sceneObjects[object.uuid] = object;

  assert(continuous_placement::CloneElement(
      scene, ContinuousPlacementType::Fixture, "fixture-1", "fixture-2"));
  assert(scene.fixtures.at("fixture-2").fixtureId == 8);
  assert(continuous_placement::CloneElement(
      scene, ContinuousPlacementType::Truss, "truss-1", "truss-2"));
  assert(continuous_placement::CloneElement(
      scene, ContinuousPlacementType::SceneObject, "object-1", "object-2"));

  assert(continuous_placement::PositionMeters(
             scene, ContinuousPlacementType::Fixture, "fixture-1") ==
         (std::array<float, 3>{1.0f, 2.0f, 3.0f}));
  assert(continuous_placement::PositionMeters(
             scene, ContinuousPlacementType::Truss, "truss-1") ==
         (std::array<float, 3>{4.0f, 5.0f, 6.0f}));
  assert(continuous_placement::PositionMeters(
             scene, ContinuousPlacementType::SceneObject, "object-1") ==
         (std::array<float, 3>{7.0f, 8.0f, 9.0f}));

  continuous_placement::EraseElement(scene, ContinuousPlacementType::Fixture,
                                     "fixture-2");
  continuous_placement::EraseElement(scene, ContinuousPlacementType::Truss,
                                     "truss-2");
  continuous_placement::EraseElement(
      scene, ContinuousPlacementType::SceneObject, "object-2");
  assert(!continuous_placement::Contains(
      scene, ContinuousPlacementType::Fixture, "fixture-2"));
  assert(!continuous_placement::Contains(scene, ContinuousPlacementType::Truss,
                                         "truss-2"));
  assert(!continuous_placement::Contains(
      scene, ContinuousPlacementType::SceneObject, "object-2"));
}
