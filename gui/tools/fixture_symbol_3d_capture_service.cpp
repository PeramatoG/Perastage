#include "tools/fixture_symbol_3d_capture_service.h"

#include "mainwindow.h"
#include "viewer2doffscreenrenderer.h"

namespace tools {

// Captures orthographic fixture symbols through a short-lived renderer instance per request.
SceneModelSymbolCaptureResult CaptureFixtureSymbolsFromDedicatedRenderer(
    MainWindow &window, ConfigManager &cfg, const std::string &fixtureUuid,
    const SceneModelSymbolCaptureOptions &options) {
  Viewer2DOffscreenRenderer isolatedRenderer(&window);
  return CaptureSceneModelOrthographicSymbols(
      isolatedRenderer, cfg,
      SceneModelSymbolTarget{SceneModelKind::Fixture, fixtureUuid}, options);
}

} // namespace tools
