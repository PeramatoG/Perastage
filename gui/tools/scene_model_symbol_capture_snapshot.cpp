#include "tools/scene_model_symbol_capture_snapshot.h"

#include "matrixutils.h"
#include "mvrscene.h"

namespace tools {

// Copies one requested model into an isolated capture-only scene snapshot.
SceneDataManager::SceneSnapshot BuildSceneModelSymbolCaptureSnapshot(
    const MvrScene &scene, const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options) {
  SceneDataManager::SceneSnapshot snapshot;
  auto alignTransform = [](const Matrix &source) {
    const Matrix identity = MatrixUtils::Identity();
    return MatrixUtils::ApplyRotationPreservingScale(source, identity,
                                                     source.o);
  };
  switch (target.kind) {
  case SceneModelKind::Fixture: {
    auto it = scene.fixtures.find(target.uuid);
    if (it != scene.fixtures.end()) {
      Fixture fixture = it->second;
      if (options.alignToLocalAxes)
        fixture.transform = alignTransform(fixture.transform);
      if (options.forcedFixtureColor)
        fixture.visualColorHex = *options.forcedFixtureColor;
      snapshot.fixtures.emplace(it->first, std::move(fixture));
    }
    break;
  }
  case SceneModelKind::Truss: {
    auto it = scene.trusses.find(target.uuid);
    if (it != scene.trusses.end()) {
      Truss truss = it->second;
      if (options.alignToLocalAxes)
        truss.transform = alignTransform(truss.transform);
      snapshot.trusses.emplace(it->first, std::move(truss));
    }
    break;
  }
  case SceneModelKind::SceneObject: {
    auto it = scene.sceneObjects.find(target.uuid);
    if (it != scene.sceneObjects.end()) {
      SceneObject object = it->second;
      if (options.alignToLocalAxes)
        object.transform = alignTransform(object.transform);
      snapshot.sceneObjects.emplace(it->first, std::move(object));
    }
    break;
  }
  }
  return snapshot;
}

// Runs one controlled render operation against an immutable capture snapshot.
bool ExecuteSceneModelSymbolCaptureBoundary(
    const MvrScene &scene, const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options,
    const std::function<bool(const SceneDataManager::SceneSnapshot &)>
        &operation) {
  const SceneDataManager::SceneSnapshot snapshot =
      BuildSceneModelSymbolCaptureSnapshot(scene, target, options);
  return ExecuteSceneModelSymbolCaptureBoundary(snapshot, operation);
}

// Runs one render operation against a previously captured immutable snapshot.
bool ExecuteSceneModelSymbolCaptureBoundary(
    const SceneDataManager::SceneSnapshot &snapshot,
    const std::function<bool(const SceneDataManager::SceneSnapshot &)>
        &operation) {
  SceneDataManager::ScopedSnapshot isolatedScene(snapshot);
  return operation(snapshot);
}

} // namespace tools
