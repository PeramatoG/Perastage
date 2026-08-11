#include "tools/scoped_single_model_capture_scene.h"

#include "configmanager.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {

// Compares matrices exactly to verify that project transforms are restored.
bool SameMatrix(const Matrix &left, const Matrix &right) {
  return left.u == right.u && left.v == right.v && left.w == right.w &&
         left.o == right.o;
}

// Returns the length of one transform basis vector.
float AxisLength(const std::array<float, 3> &axis) {
  return std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
}

} // namespace

// Verifies atomic target isolation and exact restoration of a populated scene.
int main() {
  ConfigManager &cfg = ConfigManager::Get();
  MvrScene &scene = cfg.GetScene();
  scene = {};

  Fixture target;
  target.uuid = "target";
  target.transform.u = {0.0f, 2.0f, 0.0f};
  target.transform.v = {-3.0f, 0.0f, 0.0f};
  target.transform.w = {0.0f, 0.0f, 4.0f};
  target.transform.o = {10.0f, 20.0f, 30.0f};
  scene.fixtures.emplace(target.uuid, target);
  Fixture unrelated;
  unrelated.uuid = "unrelated";
  scene.fixtures.emplace(unrelated.uuid, unrelated);
  Truss truss;
  truss.uuid = "truss";
  scene.trusses.emplace(truss.uuid, truss);
  SceneObject object;
  object.uuid = "object";
  scene.sceneObjects.emplace(object.uuid, object);
  Support support;
  support.uuid = "support";
  scene.supports.emplace(support.uuid, support);

  const MvrScene original = scene;
  {
    tools::ScopedSingleModelCaptureScene isolated(
        cfg, {tools::SceneModelKind::Fixture, target.uuid}, true);
    assert(scene.fixtures.size() == 1);
    assert(scene.fixtures.contains(target.uuid));
    assert(scene.trusses.empty());
    assert(scene.sceneObjects.empty());
    assert(scene.supports.empty());
    const Matrix &aligned = scene.fixtures.at(target.uuid).transform;
    assert(aligned.o == target.transform.o);
    assert(!SameMatrix(aligned, target.transform));
    assert(AxisLength(aligned.u) == AxisLength(target.transform.u));
    assert(AxisLength(aligned.v) == AxisLength(target.transform.v));
    assert(AxisLength(aligned.w) == AxisLength(target.transform.w));
  }

  assert(scene.fixtures.size() == original.fixtures.size());
  assert(scene.trusses.size() == original.trusses.size());
  assert(scene.sceneObjects.size() == original.sceneObjects.size());
  assert(scene.supports.size() == original.supports.size());
  assert(SameMatrix(scene.fixtures.at(target.uuid).transform,
                    original.fixtures.at(target.uuid).transform));

  {
    tools::ScopedSingleModelCaptureScene missing(
        cfg, {tools::SceneModelKind::Fixture, "missing"}, true);
    assert(scene.fixtures.empty());
    assert(scene.trusses.empty());
    assert(scene.sceneObjects.empty());
    assert(scene.supports.empty());
  }
  assert(scene.fixtures.size() == original.fixtures.size());
  assert(scene.trusses.size() == original.trusses.size());
  assert(scene.sceneObjects.size() == original.sceneObjects.size());
  assert(scene.supports.size() == original.supports.size());
  return 0;
}
