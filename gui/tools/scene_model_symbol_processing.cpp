#include "tools/scene_model_symbol_processing.h"

#include "symbols/SymbolGeometrySimplifier.h"

namespace tools {
namespace {

constexpr float kSymbolRdpEpsilon = 1.0f;

} // namespace

// Vectorizes, simplifies, and validates a complete four-view capture set.
SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders(
    std::vector<symbols::RenderedSymbolImage> renders,
    const FixtureGeometryBounds &fixtureBoundsMm,
    symbols::FixtureSymbolTimings *timings) {
  SceneModelSymbolCaptureResult result;
  result.fixtureBoundsMm = fixtureBoundsMm;
  const auto &requests = symbols::FixtureSymbolCapturePlan();
  if (renders.size() != requests.size()) {
    result.error = "Could not generate all symbols from captured views.";
    return result;
  }
  {
    symbols::ScopedFixtureSymbolPhase phase(
        timings, symbols::FixtureSymbolPhase::Vectorization);
    result.symbols =
        symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders);
    for (auto &symbol : result.symbols)
      symbols::SimplifySymbolGeometry(symbol, kSymbolRdpEpsilon);
  }
  result.ok = result.symbols.size() == requests.size();
  if (!result.ok)
    result.error = "Could not generate all symbols from captured views.";
  return result;
}

} // namespace tools
