#include "scene_transform_integrity.h"

#include "matrixutils.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace {

// Builds a translated and rotated matrix for integrity test fixtures.
Matrix Transform(float x, float y, float z, float yaw = 0.0f) {
  Matrix matrix = MatrixUtils::EulerToMatrix(yaw, 0.0f, 0.0f);
  matrix.o = {x, y, z};
  return matrix;
}

// Builds one valid grouped fixture scene with canonical local data.
MvrScene ValidScene() {
  MvrScene scene;
  GroupObject group;
  group.uuid = "group";
  group.transform = Transform(100.0f, 200.0f, 300.0f, 25.0f);
  group.localTransform = group.transform;
  group.children.push_back({MvrNodeType::Fixture, "fixture"});
  scene.groupObjects[group.uuid] = group;

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.parentGroupUuid = group.uuid;
  fixture.localTransform = Transform(50.0f, 0.0f, 0.0f, 10.0f);
  fixture.hasLocalTransform = true;
  fixture.transform = MatrixUtils::Multiply(group.transform, fixture.localTransform);
  scene.fixtures[fixture.uuid] = fixture;
  return scene;
}

// Finds whether a result contains the requested diagnostic reason.
bool HasReason(const scene_transform_integrity::Result &result,
               const std::string &reason) {
  for (const auto &diagnostic : result.diagnostics) {
    if (diagnostic.reason == reason)
      return true;
  }
  return false;
}

// Compares every component of two matrices with the integrity tolerance.
bool MatrixEqual(const Matrix &lhs, const Matrix &rhs) {
  for (const auto &pair : {std::pair{&lhs.u, &rhs.u},
                           std::pair{&lhs.v, &rhs.v},
                           std::pair{&lhs.w, &rhs.w},
                           std::pair{&lhs.o, &rhs.o}}) {
    for (size_t index = 0; index < 3; ++index) {
      if (std::fabs((*pair.first)[index] - (*pair.second)[index]) >= 0.001f)
        return false;
    }
  }
  return true;
}

} // namespace

// Verifies canonical hierarchy validation, safe repairs, and fatal corruption.
int main() {
  MvrScene valid = ValidScene();
  auto result = scene_transform_integrity::ValidateAndRepair(valid);
  assert(result.success && !result.repaired && result.diagnostics.empty());

  MvrScene stale = ValidScene();
  stale.fixtures["fixture"].localTransform = MatrixUtils::Identity();
  result = scene_transform_integrity::ValidateAndRepair(stale);
  assert(result.success && result.repaired);
  assert(HasReason(result, "stale local transform"));
  const Matrix rebuilt = MatrixUtils::Multiply(
      stale.groupObjects["group"].transform,
      stale.fixtures["fixture"].localTransform);
  assert(MatrixEqual(rebuilt, stale.fixtures["fixture"].transform));

  MvrScene missingLocal = ValidScene();
  missingLocal.fixtures["fixture"].hasLocalTransform = false;
  result = scene_transform_integrity::ValidateAndRepair(missingLocal);
  assert(result.success && result.repaired);
  assert(missingLocal.fixtures["fixture"].hasLocalTransform);
  assert(HasReason(result, "missing local transform"));

  MvrScene nested = ValidScene();
  GroupObject childGroup;
  childGroup.uuid = "nested";
  childGroup.parentGroupUuid = "group";
  childGroup.localTransform = Transform(0.0f, 75.0f, 20.0f, 15.0f);
  childGroup.transform = MatrixUtils::Multiply(
      nested.groupObjects["group"].transform, childGroup.localTransform);
  nested.groupObjects["group"].children.push_back(
      {MvrNodeType::GroupObject, childGroup.uuid});
  nested.groupObjects[childGroup.uuid] = childGroup;
  nested.groupObjects["nested"].localTransform = MatrixUtils::Identity();
  result = scene_transform_integrity::ValidateAndRepair(nested);
  assert(result.success && result.repaired);
  assert(nested.groupObjects["nested"].parentGroupUuid == "group");

  MvrScene missingParent = ValidScene();
  missingParent.fixtures["fixture"].parentGroupUuid = "absent";
  result = scene_transform_integrity::ValidateAndRepair(missingParent);
  assert(!result.success && HasReason(result, "missing parent group"));

  MvrScene dangling = ValidScene();
  dangling.groupObjects["group"].children.push_back(
      {MvrNodeType::Truss, "absent"});
  result = scene_transform_integrity::ValidateAndRepair(dangling);
  assert(!result.success && HasReason(result, "dangling child reference"));

  MvrScene mismatched = ValidScene();
  mismatched.fixtures["fixture"].parentGroupUuid.clear();
  result = scene_transform_integrity::ValidateAndRepair(mismatched);
  assert(!result.success &&
         HasReason(result, "parent metadata disagrees with child reference"));

  MvrScene duplicate = ValidScene();
  duplicate.groupObjects["group"].children.push_back(
      {MvrNodeType::Fixture, "fixture"});
  result = scene_transform_integrity::ValidateAndRepair(duplicate);
  assert(!result.success &&
         HasReason(result, "duplicate or conflicting group ownership"));

  MvrScene cycle;
  GroupObject first;
  first.uuid = "a";
  first.parentGroupUuid = "b";
  first.children.push_back({MvrNodeType::GroupObject, "b"});
  GroupObject second;
  second.uuid = "b";
  second.parentGroupUuid = "a";
  second.children.push_back({MvrNodeType::GroupObject, "a"});
  cycle.groupObjects[first.uuid] = first;
  cycle.groupObjects[second.uuid] = second;
  result = scene_transform_integrity::ValidateAndRepair(cycle);
  assert(!result.success && HasReason(result, "group ancestry cycle"));

  MvrScene nonFinite = ValidScene();
  nonFinite.fixtures["fixture"].transform.o[0] =
      std::numeric_limits<float>::infinity();
  result = scene_transform_integrity::ValidateAndRepair(nonFinite);
  assert(!result.success && HasReason(result, "non-finite world transform"));

  MvrScene nonFiniteLocal = ValidScene();
  nonFiniteLocal.fixtures["fixture"].localTransform.o[1] =
      std::numeric_limits<float>::quiet_NaN();
  result = scene_transform_integrity::ValidateAndRepair(nonFiniteLocal);
  assert(!result.success && HasReason(result, "non-finite local transform"));

  MvrScene singular = ValidScene();
  singular.groupObjects["group"].transform.u = {0.0f, 0.0f, 0.0f};
  singular.groupObjects["group"].localTransform =
      singular.groupObjects["group"].transform;
  result = scene_transform_integrity::ValidateAndRepair(singular);
  assert(!result.success && HasReason(result, "non-invertible parent transform"));
  return 0;
}
