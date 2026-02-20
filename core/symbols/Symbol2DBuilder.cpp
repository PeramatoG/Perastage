#include "symbols/Symbol2DBuilder.h"

#include "symbols/ContourTracer.h"
#include "symbols/MaskUtils.h"
#include "symbols/OffscreenSymbolRenderer.h"
#include "symbols/SkeletonGraph.h"
#include "symbols/Skeletonize.h"

#include <algorithm>

namespace symbols {
namespace {
Aabb2D ComputeBounds(const Symbol2D &symbol) {
  Aabb2D bounds;
  bool first = true;
  auto addPoint = [&](const Point2D &p) {
    if (first) {
      bounds.minX = bounds.maxX = p.x;
      bounds.minY = bounds.maxY = p.y;
      first = false;
      return;
    }
    bounds.minX = std::min(bounds.minX, p.x);
    bounds.minY = std::min(bounds.minY, p.y);
    bounds.maxX = std::max(bounds.maxX, p.x);
    bounds.maxY = std::max(bounds.maxY, p.y);
  };

  for (const auto &poly : symbol.fill) {
    for (const auto &p : poly.outer)
      addPoint(p);
    for (const auto &hole : poly.holes)
      for (const auto &p : hole)
        addPoint(p);
  }
  for (const auto &line : symbol.strokes)
    for (const auto &p : line.points)
      addPoint(p);

  if (first)
    bounds = Aabb2D{};
  return bounds;
}
} // namespace

SymbolCollection Symbol2DBuilder::BuildForFixture(const std::string &fixtureTypeId,
                                                  const std::string &gdtfSpec,
                                                  const std::string &sceneBasePath,
                                                  const SymbolBuildParams &params) {
  const std::string cacheKey = fixtureTypeId + "|" + gdtfSpec + "|" + sceneBasePath;
  if (const auto cached = cache.TryGet(cacheKey, params))
    return *cached;

  SymbolCollection output;
  output.reserve(4);

  OffscreenSymbolRenderer renderer;
  for (const auto view : AllSymbolViews()) {
    const auto shapeRender = renderer.RenderFixtureTechnical(
        gdtfSpec, sceneBasePath, view, params.renderResolution,
        params.renderResolution,
        RenderMode::ShapeBlack);
    const auto lineRender = renderer.RenderFixtureTechnical(
        gdtfSpec, sceneBasePath, view, params.renderResolution,
        params.renderResolution,
        RenderMode::LinesBlackOnWhiteFill);

    auto shapeMask = ExtractShapeMask(shapeRender.rgba);
    auto lineMask = ExtractLineMask(lineRender.rgba);
    MorphClose(shapeMask, shapeRender.rgba.width, shapeRender.rgba.height);

    Symbol2D symbol;
    symbol.view = view;
    symbol.stroke_width_px = params.strokeWidthPx;
    symbol.fill = TraceFillPolygons(shapeMask, shapeRender.rgba.width,
                                    shapeRender.rgba.height,
                                    params.contourSimplify);

    auto skeleton = SkeletonizeMask(lineMask, lineRender.rgba.width,
                                    lineRender.rgba.height);
    symbol.strokes = SkeletonToPolylines(skeleton, lineRender.rgba.width,
                                         lineRender.rgba.height,
                                         params.minLineLength,
                                         params.lineSimplify);
    symbol.bounds = ComputeBounds(symbol);
    output.push_back(std::move(symbol));
  }

  cache.Store(cacheKey, params, output);
  return output;
}

} // namespace symbols
