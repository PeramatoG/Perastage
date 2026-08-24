#include "tools/scoped_single_model_capture_scene.h"

#include "configmanager.h"
#include "matrixutils.h"

#include <cassert>

namespace {

// Compares matrices exactly to verify that project transforms are restored.
bool SameMatrix(const Matrix &left, const Matrix &right) {
  return left.u == right.u && left.v == right.v && left.w == right.w &&
         left.o == right.o;
}

// Returns the isolated fixture clone produced for one representative UUID.
Fixture CaptureFixtureClone(ConfigManager &cfg, const std::string &uuid) {
  tools::ScopedSingleModelCaptureScene isolated(
      cfg, {tools::SceneModelKind::Fixture, uuid},
      tools::SymbolCaptureTransformPolicy::CanonicalFixtureType);
  return cfg.GetScene().fixtures.at(uuid);
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
  target.parentGroupUuid = "parent";
  target.hasLocalTransform = true;
  target.localTransform = target.transform;
  GroupObject parentGroup;
  parentGroup.uuid = target.parentGroupUuid;
  parentGroup.transform.u = {0.0f, 1.0f, 0.0f};
  parentGroup.transform.v = {-1.0f, 0.0f, 0.0f};
  parentGroup.transform.o = {500.0f, -300.0f, 80.0f};
  scene.groupObjects.emplace(parentGroup.uuid, parentGroup);
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
        cfg, {tools::SceneModelKind::Fixture, target.uuid},
        tools::SymbolCaptureTransformPolicy::CanonicalFixtureType);
    assert(scene.fixtures.size() == 1);
    assert(scene.fixtures.contains(target.uuid));
    assert(scene.trusses.empty());
    assert(scene.sceneObjects.empty());
    assert(scene.supports.empty());
    const Matrix &aligned = scene.fixtures.at(target.uuid).transform;
    assert(SameMatrix(aligned, MatrixUtils::Identity()));
    assert(scene.fixtures.at(target.uuid).parentGroupUuid.empty());
    assert(!scene.fixtures.at(target.uuid).hasLocalTransform);
    assert(SameMatrix(scene.fixtures.at(target.uuid).localTransform,
                      MatrixUtils::Identity()));
  }

  assert(scene.fixtures.size() == original.fixtures.size());
  assert(scene.trusses.size() == original.trusses.size());
  assert(scene.sceneObjects.size() == original.sceneObjects.size());
  assert(scene.supports.size() == original.supports.size());
  assert(SameMatrix(scene.fixtures.at(target.uuid).transform,
                    original.fixtures.at(target.uuid).transform));

  {
    tools::ScopedSingleModelCaptureScene missing(
        cfg, {tools::SceneModelKind::Fixture, "missing"},
        tools::SymbolCaptureTransformPolicy::CanonicalFixtureType);
    assert(scene.fixtures.empty());
    assert(scene.trusses.empty());
    assert(scene.sceneObjects.empty());
    assert(scene.supports.empty());
  }
  assert(scene.fixtures.size() == original.fixtures.size());
  assert(scene.trusses.size() == original.trusses.size());
  assert(scene.sceneObjects.size() == original.sceneObjects.size());
  assert(scene.supports.size() == original.supports.size());

  Fixture firstRepresentative = target;
  firstRepresentative.uuid = "000-first";
  Fixture secondRepresentative = target;
  secondRepresentative.uuid = "zzz-second";
  secondRepresentative.transform.u = {2.0f, 0.3f, 0.0f};
  secondRepresentative.transform.v = {0.4f, 0.5f, 0.2f};
  secondRepresentative.transform.w = {0.1f, 0.0f, 1.4f};
  secondRepresentative.transform.o = {-900.0f, 700.0f, 120.0f};
  secondRepresentative.parentGroupUuid = "different-parent";
  secondRepresentative.localTransform = secondRepresentative.transform;
  secondRepresentative.hasLocalTransform = true;
  scene.fixtures = {{firstRepresentative.uuid, firstRepresentative},
                    {secondRepresentative.uuid, secondRepresentative}};
  const Fixture firstClone = CaptureFixtureClone(cfg, firstRepresentative.uuid);
  const Fixture secondClone =
      CaptureFixtureClone(cfg, secondRepresentative.uuid);
  assert(SameMatrix(firstClone.transform, secondClone.transform));
  assert(firstClone.parentGroupUuid == secondClone.parentGroupUuid);
  assert(firstClone.hasLocalTransform == secondClone.hasLocalTransform);
  assert(SameMatrix(firstClone.localTransform, secondClone.localTransform));

  MvrScene replacementScene;
  replacementScene.fixtures.emplace(secondRepresentative.uuid,
                                    secondRepresentative);
  MvrScene priorScene;
  priorScene.fixtures.emplace(firstRepresentative.uuid, firstRepresentative);
  scene = priorScene;
  scene = replacementScene;
  const Fixture afterReplacement =
      CaptureFixtureClone(cfg, secondRepresentative.uuid);
  scene = replacementScene;
  const Fixture freshScene = CaptureFixtureClone(cfg, secondRepresentative.uuid);
  assert(SameMatrix(afterReplacement.transform, freshScene.transform));
  assert(afterReplacement.parentGroupUuid == freshScene.parentGroupUuid);
  assert(afterReplacement.hasLocalTransform == freshScene.hasLocalTransform);
  return 0;
}
