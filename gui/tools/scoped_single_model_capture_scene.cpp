#include "tools/scoped_single_model_capture_scene.h"

#include "configmanager.h"
#include "matrixutils.h"

namespace tools {

// Replaces renderable scene containers with one aligned capture target.
ScopedSingleModelCaptureScene::ScopedSingleModelCaptureScene(
    ConfigManager &cfg, const SceneModelSymbolTarget &target,
    SymbolCaptureTransformPolicy transformPolicy)
    : cfg_(cfg) {
  auto &scene = cfg_.GetScene();
  originalFixtures_.swap(scene.fixtures);
  originalTrusses_.swap(scene.trusses);
  originalSceneObjects_.swap(scene.sceneObjects);
  originalSupports_.swap(scene.supports);

  try {
    switch (target.kind) {
    case SceneModelKind::Fixture: {
      const auto it = originalFixtures_.find(target.uuid);
      if (it != originalFixtures_.end()) {
        Fixture fixture = it->second;
        if (transformPolicy ==
            SymbolCaptureTransformPolicy::CanonicalFixtureType) {
          fixture.transform = MatrixUtils::Identity();
          fixture.parentGroupUuid.clear();
          fixture.hasLocalTransform = false;
          fixture.localTransform = MatrixUtils::Identity();
        } else if (transformPolicy ==
                   SymbolCaptureTransformPolicy::AlignRotationPreserveScale) {
          fixture.transform = AlignTransform(fixture.transform);
        }
        scene.fixtures.emplace(it->first, std::move(fixture));
      }
      break;
    }
    case SceneModelKind::Truss: {
      const auto it = originalTrusses_.find(target.uuid);
      if (it != originalTrusses_.end()) {
        Truss truss = it->second;
        if (transformPolicy ==
            SymbolCaptureTransformPolicy::AlignRotationPreserveScale)
          truss.transform = AlignTransform(truss.transform);
        scene.trusses.emplace(it->first, std::move(truss));
      }
      break;
    }
    case SceneModelKind::SceneObject: {
      const auto it = originalSceneObjects_.find(target.uuid);
      if (it != originalSceneObjects_.end()) {
        SceneObject object = it->second;
        if (transformPolicy ==
            SymbolCaptureTransformPolicy::AlignRotationPreserveScale)
          object.transform = AlignTransform(object.transform);
        scene.sceneObjects.emplace(it->first, std::move(object));
      }
      break;
    }
    }
  } catch (...) {
    RestoreScene();
    throw;
  }
}

// Restores every live renderable scene container exactly once.
ScopedSingleModelCaptureScene::~ScopedSingleModelCaptureScene() {
  RestoreScene();
}

// Restores the saved containers without allocating or throwing.
void ScopedSingleModelCaptureScene::RestoreScene() noexcept {
  auto &scene = cfg_.GetScene();
  scene.fixtures.clear();
  scene.trusses.clear();
  scene.sceneObjects.clear();
  scene.supports.clear();
  scene.fixtures.swap(originalFixtures_);
  scene.trusses.swap(originalTrusses_);
  scene.sceneObjects.swap(originalSceneObjects_);
  scene.supports.swap(originalSupports_);
}

// Removes rotation while preserving translation and scale.
Matrix ScopedSingleModelCaptureScene::AlignTransform(const Matrix &source) {
  const Matrix identity = MatrixUtils::Identity();
  return MatrixUtils::ApplyRotationPreservingScale(source, identity, source.o);
}

} // namespace tools
