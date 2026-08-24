#pragma once

#include <optional>
#include <string>
#include <vector>

#include <wx/gdicmn.h>

#include "symbols/FixtureSymbolDiagnostics.h"
#include "symbols/Symbol2D.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "tools/fixture_geometry_bounds.h"
#include "tools/scene_model_symbol_target.h"
#include "tools/symbol_capture_transform_policy.h"

class ConfigManager;
class Viewer2DOffscreenRenderer;

namespace tools {

struct SceneModelSymbolCaptureOptions {
  SymbolCaptureTransformPolicy transformPolicy =
      SymbolCaptureTransformPolicy::PreserveInstanceTransform;
  std::string diagnosticOrigin;
  std::optional<std::string> forcedFixtureColor;
  wxSize viewportSize = wxSize(1200, 1200);
  std::optional<FixtureGeometryBounds> fixtureBoundsOverride;
  symbols::FixtureSymbolTimings *timings = nullptr;
};

SceneModelSymbolCaptureOptions BuildFixtureTypeSymbolCaptureOptions(
    std::string diagnosticOrigin);

struct SceneModelSymbolCaptureResult {
  bool ok = false;
  std::string error;
  std::vector<symbols::Symbol2D> symbols;
  FixtureGeometryBounds fixtureBoundsMm;
};

struct SceneModelSymbolRenderResult {
  bool ok = false;
  std::string error;
  std::vector<symbols::RenderedSymbolImage> renders;
  FixtureGeometryBounds fixtureBoundsMm;
};

SceneModelSymbolRenderResult CaptureSceneModelOrthographicRenders(
    Viewer2DOffscreenRenderer &renderer, ConfigManager &cfg,
    const SceneModelSymbolTarget &target,
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
