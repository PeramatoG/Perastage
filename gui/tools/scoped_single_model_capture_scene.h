#pragma once

#include <string>
#include <unordered_map>

#include "fixture.h"
#include "sceneobject.h"
#include "support.h"
#include "tools/scene_model_symbol_target.h"
#include "truss.h"

class ConfigManager;

namespace tools {

// Provides the narrow legacy-compatible scene isolation required by symbol capture.
class ScopedSingleModelCaptureScene {
public:
  ScopedSingleModelCaptureScene(ConfigManager &cfg,
                                const SceneModelSymbolTarget &target,
                                bool alignToLocalAxes);
  ~ScopedSingleModelCaptureScene();

  ScopedSingleModelCaptureScene(const ScopedSingleModelCaptureScene &) = delete;
  ScopedSingleModelCaptureScene &
  operator=(const ScopedSingleModelCaptureScene &) = delete;

private:
  static Matrix AlignTransform(const Matrix &source);
  void RestoreScene() noexcept;

  ConfigManager &cfg_;
  std::unordered_map<std::string, Fixture> originalFixtures_;
  std::unordered_map<std::string, Truss> originalTrusses_;
  std::unordered_map<std::string, SceneObject> originalSceneObjects_;
  std::unordered_map<std::string, Support> originalSupports_;
};

} // namespace tools
