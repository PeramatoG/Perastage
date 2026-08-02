#include "tools/symbol_physical_calibration.h"

#include <algorithm>

namespace tools {
namespace {

struct Bounds2D {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  bool valid = false;
};

// Projects physical 3D bounds onto the plane associated with a symbol view.
Bounds2D BuildTargetBounds(const FixtureGeometryBounds &bounds,
                           symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Top:
  case symbols::SymbolView::Bottom:
    return {bounds.min[0], bounds.min[1], bounds.max[0], bounds.max[1], true};
  case symbols::SymbolView::Front:
    return {bounds.min[0], bounds.min[2], bounds.max[0], bounds.max[2], true};
  case symbols::SymbolView::Left:
    return {bounds.min[1], bounds.min[2], bounds.max[1], bounds.max[2], true};
  }
  return {};
}

// Uniformly scales and centers a symbol into physical target bounds.
bool RemapSymbolToBounds(symbols::Symbol2D &symbol, const Bounds2D &target) {
  if (!symbol.bounds.valid || !target.valid)
    return false;
  const float sourceWidth = symbol.bounds.max.x - symbol.bounds.min.x;
  const float sourceHeight = symbol.bounds.max.y - symbol.bounds.min.y;
  const float targetWidth = target.maxX - target.minX;
  const float targetHeight = target.maxY - target.minY;
  if (sourceWidth <= 0.0f || sourceHeight <= 0.0f || targetWidth <= 0.0f ||
      targetHeight <= 0.0f)
    return false;
  const float scale =
      std::min(targetWidth / sourceWidth, targetHeight / sourceHeight);
  const float offsetX =
      target.minX + (targetWidth - sourceWidth * scale) * 0.5f;
  const float offsetY =
      target.minY + (targetHeight - sourceHeight * scale) * 0.5f;
  auto remap = [&](symbols::Point2D &point) {
    point.x = offsetX + (point.x - symbol.bounds.min.x) * scale;
    point.y = offsetY + (point.y - symbol.bounds.min.y) * scale;
  };
  for (auto &polygon : symbol.fill) {
    for (auto &point : polygon.outer)
      remap(point);
    for (auto &hole : polygon.holes)
      for (auto &point : hole)
        remap(point);
  }
  for (auto &stroke : symbol.strokes)
    for (auto &point : stroke)
      remap(point);
  symbol.bounds = {
      {target.minX, target.minY}, {target.maxX, target.maxY}, true};
  return true;
}

} // namespace

// Maps generated coordinates to precomputed fixture physical dimensions.
bool CalibrateFixtureSymbolsToPhysicalUnits(
    const FixtureGeometryBounds &fixtureBounds,
    std::vector<symbols::Symbol2D> &symbols, std::string &errorMessage) {
  bool remappedAny = false;
  for (auto &symbol : symbols) {
    if (symbol.bounds.valid)
      remappedAny =
          RemapSymbolToBounds(symbol,
                              BuildTargetBounds(fixtureBounds, symbol.view)) ||
          remappedAny;
  }
  if (!remappedAny) {
    errorMessage =
        "No valid symbol views were available for physical calibration.";
    return false;
  }
  return true;
}

} // namespace tools
