#pragma once

#include <optional>
#include <string>
#include <vector>

#include <wx/gdicmn.h>

#include "symbols/FixtureSymbolDiagnostics.h"
#include "symbols/Symbol2D.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "tools/fixture_geometry_bounds.h"

class ConfigManager;
class Viewer2DOffscreenRenderer;

namespace tools {

enum class SceneModelKind {
  Fixture,
  Truss,
  SceneObject,
};

struct SceneModelSymbolTarget {
  SceneModelKind kind = SceneModelKind::Fixture;
  std::string uuid;
};

struct SceneModelSymbolCaptureOptions {
  bool alignToLocalAxes = false;
  std::optional<std::string> forcedFixtureColor;
  wxSize viewportSize = wxSize(1200, 1200);
  symbols::FixtureSymbolTimings *timings = nullptr;
};

struct SceneModelSymbolCaptureResult {
  bool ok = false;
  std::string error;
  std::vector<symbols::Symbol2D> symbols;
  FixtureGeometryBounds fixtureBoundsMm;
};

struct SceneModelSymbolCaptureStepResult {
  bool ok = false;
  std::string error;
  std::optional<symbols::RenderedSymbolImage> image;
  FixtureGeometryBounds fixtureBoundsMm;
};

SceneModelSymbolCaptureStepResult CaptureSceneModelOrthographicStep(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target, std::size_t stepIndex,
    const SceneModelSymbolCaptureOptions &options = {});

SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders(
    std::vector<symbols::RenderedSymbolImage> renders,
    const FixtureGeometryBounds &fixtureBoundsMm,
    symbols::FixtureSymbolTimings *timings = nullptr);

SceneModelSymbolCaptureResult CaptureSceneModelOrthographicSymbols(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
                                     const SceneModelSymbolTarget &target,
                                     const SceneModelSymbolCaptureOptions &options = {});

} // namespace tools
