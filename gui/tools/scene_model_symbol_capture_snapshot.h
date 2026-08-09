#pragma once

#include <functional>

#include "scenedatamanager.h"
#include "tools/scene_model_symbol_capture_service.h"

class MvrScene;

namespace tools {

// Copies one requested model into an isolated capture-only scene snapshot.
SceneDataManager::SceneSnapshot BuildSceneModelSymbolCaptureSnapshot(
    const MvrScene &scene, const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options);

bool ExecuteSceneModelSymbolCaptureBoundary(
    const MvrScene &scene, const SceneModelSymbolTarget &target,
    const SceneModelSymbolCaptureOptions &options,
    const std::function<bool(const SceneDataManager::SceneSnapshot &)>
        &operation);

bool ExecuteSceneModelSymbolCaptureBoundary(
    const SceneDataManager::SceneSnapshot &snapshot,
    const std::function<bool(const SceneDataManager::SceneSnapshot &)>
        &operation);

} // namespace tools
