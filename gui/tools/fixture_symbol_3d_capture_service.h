#pragma once

#include <string>

#include "tools/scene_model_symbol_capture_service.h"

class MainWindow;
class Viewer2DOffscreenRenderer;
class ConfigManager;

namespace tools {

// Captures fixture symbols using a dedicated offscreen renderer isolated from shared UI state.
SceneModelSymbolCaptureResult CaptureFixtureSymbolsFromDedicatedRenderer(
    MainWindow &window, ConfigManager &cfg, const std::string &fixtureUuid,
    const SceneModelSymbolCaptureOptions &options);

} // namespace tools
