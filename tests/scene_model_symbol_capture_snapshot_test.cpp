#include "tools/scene_model_symbol_capture_snapshot.h"

#include "mvrscene.h"

#include <cassert>

namespace {

// Compares transforms exactly to prove capture preparation did not mutate live
// data.
bool SameMatrix(const Matrix &left, const Matrix &right) {
  return left.u == right.u && left.v == right.v && left.w == right.w &&
         left.o == right.o;
}

} // namespace

// Verifies capture snapshots isolate all live scene categories on success and
// failure.
int main() {
  MvrScene scene;
  scene.basePath = "/project/source";
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.visualColorHex = "#112233";
  fixture.transform.u = {0.0f, 2.0f, 0.0f};
  fixture.transform.v = {-3.0f, 0.0f, 0.0f};
  fixture.transform.o = {10.0f, 20.0f, 30.0f};
  scene.fixtures.emplace(fixture.uuid, fixture);
  Truss truss;
  truss.uuid = "truss";
  scene.trusses.emplace(truss.uuid, truss);
  SceneObject object;
  object.uuid = "object";
  scene.sceneObjects.emplace(object.uuid, object);
  Support support;
  support.uuid = "support";
  scene.supports.emplace(support.uuid, support);

  const Matrix originalTransform = scene.fixtures.at("fixture").transform;
  tools::SceneModelSymbolCaptureOptions options;
  options.alignToLocalAxes = true;
  options.forcedFixtureColor = "#FFFFFF";
  const auto snapshot = tools::BuildSceneModelSymbolCaptureSnapshot(
      scene, {tools::SceneModelKind::Fixture, "fixture"}, options);
  assert(snapshot.fixtures.size() == 1);
  assert(snapshot.basePath == scene.basePath);
  assert(snapshot.trusses.empty());
  assert(snapshot.sceneObjects.empty());
  assert(snapshot.fixtures.at("fixture").visualColorHex == "#FFFFFF");

  assert(scene.fixtures.size() == 1);
  assert(scene.trusses.size() == 1);
  assert(scene.sceneObjects.size() == 1);
  assert(scene.supports.size() == 1);
  assert(scene.fixtures.at("fixture").visualColorHex == "#112233");
  assert(SameMatrix(scene.fixtures.at("fixture").transform, originalTransform));

  scene.fixtures.at("fixture").visualColorHex = "#445566";
  scene.fixtures.at("fixture").transform.o = {90.0f, 80.0f, 70.0f};
  assert(tools::ExecuteSceneModelSymbolCaptureBoundary(
      snapshot, [&](const SceneDataManager::SceneSnapshot &stable) {
        assert(stable.fixtures.at("fixture").visualColorHex == "#FFFFFF");
        assert(stable.fixtures.at("fixture").transform.o !=
               scene.fixtures.at("fixture").transform.o);
        assert(SceneDataManager::Instance().HasActiveSnapshot());
        assert(SceneDataManager::Instance().GetBasePath() == scene.basePath);
        assert(SceneDataManager::Instance().GetFixtures().find("fixture") !=
               SceneDataManager::Instance().GetFixtures().end());
        assert(SceneDataManager::Instance().GetTrusses().empty());
        assert(SceneDataManager::Instance().GetSceneObjects().empty());
        assert(SceneDataManager::Instance().IsFixtureTypeVisible("hidden-type"));
        return true;
      }));
  assert(!SceneDataManager::Instance().HasActiveSnapshot());
  scene.fixtures.at("fixture").visualColorHex = "#112233";
  scene.fixtures.at("fixture").transform = originalTransform;

  int attempts = 0;
  const auto runBoundary = [&](bool succeeds) {
    return tools::ExecuteSceneModelSymbolCaptureBoundary(
        scene, {tools::SceneModelKind::Fixture, "fixture"}, options,
        [&](const SceneDataManager::SceneSnapshot &isolated) {
          ++attempts;
          assert(isolated.fixtures.at("fixture").visualColorHex == "#FFFFFF");
          assert(SceneDataManager::Instance().GetFixtures().size() == 1);
          return succeeds;
        });
  };
  assert(runBoundary(true));
  assert(!runBoundary(false));
  assert(attempts == 2);
  assert(scene.fixtures.size() == 1);
  assert(scene.trusses.size() == 1);
  assert(scene.sceneObjects.size() == 1);
  assert(scene.supports.size() == 1);
  assert(scene.fixtures.at("fixture").visualColorHex == "#112233");
  assert(SameMatrix(scene.fixtures.at("fixture").transform, originalTransform));

  const auto failedSnapshot = tools::BuildSceneModelSymbolCaptureSnapshot(
      scene, {tools::SceneModelKind::Fixture, "missing"}, options);
  assert(failedSnapshot.fixtures.empty());
  assert(scene.fixtures.size() == 1);
  assert(scene.trusses.size() == 1);
  assert(scene.sceneObjects.size() == 1);
  assert(scene.supports.size() == 1);
  assert(scene.fixtures.at("fixture").visualColorHex == "#112233");
  assert(SameMatrix(scene.fixtures.at("fixture").transform, originalTransform));
  return 0;
}
