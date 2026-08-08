#pragma once

#include "scenedatamanager.h"
#include "tools/scene_model_symbol_capture_service.h"

class MvrScene;

namespace tools {

// Copies one requested model into an isolated capture-only scene snapshot.
SceneDataManager::SceneSnapshot BuildSceneModelSymbolCaptureSnapshot(
    const MvrScene &scene, const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options);

} // namespace tools
