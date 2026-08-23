#pragma once

#include <string>
#include <vector>

#include "symbols/FixtureSymbolDiagnostics.h"
#include "symbols/Symbol2D.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "tools/fixture_geometry_bounds.h"

namespace tools {

struct SceneModelSymbolCaptureResult {
  bool ok = false;
  std::string error;
  std::vector<symbols::Symbol2D> symbols;
  FixtureGeometryBounds fixtureBoundsMm;
};

SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders(
    std::vector<symbols::RenderedSymbolImage> renders,
    const FixtureGeometryBounds &fixtureBoundsMm,
    symbols::FixtureSymbolTimings *timings = nullptr);

} // namespace tools
