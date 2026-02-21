#include "symbols/Symbol2DBuilder.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <type_traits>

#include "canvas2d.h"
#include "symbolcache.h"

namespace symbols {
namespace {

std::optional<SymbolView> ToSymbolView(SymbolViewKind viewKind) {
  switch (viewKind) {
  case SymbolViewKind::Front:
    return SymbolView::Front;
  case SymbolViewKind::Top:
    return SymbolView::Top;
  case SymbolViewKind::Bottom:
    return SymbolView::Bottom;
  case SymbolViewKind::Left:
  case SymbolViewKind::Right:
    return SymbolView::Left;
  default:
    return std::nullopt;
  }
}

void ExtendBounds(Aabb2D &bounds, const Point2D &p) {
  if (!bounds.valid) {
    bounds.min = p;
    bounds.max = p;
    bounds.valid = true;
    return;
  }
  bounds.min.x = std::min(bounds.min.x, p.x);
  bounds.min.y = std::min(bounds.min.y, p.y);
  bounds.max.x = std::max(bounds.max.x, p.x);
  bounds.max.y = std::max(bounds.max.y, p.y);
}

Polyline2D ToPolyline(const std::vector<float> &points) {
  Polyline2D polyline;
  polyline.reserve(points.size() / 2);
  for (size_t i = 0; i + 1 < points.size(); i += 2)
    polyline.push_back(Point2D{points[i], points[i + 1]});
  return polyline;
}

bool HasModelKey(const std::vector<std::string> &modelKeys,
                 const std::string &modelKey) {
  return std::find(modelKeys.begin(), modelKeys.end(), modelKey) !=
         modelKeys.end();
}

} // namespace

std::vector<Symbol2D>
Symbol2DBuilder::BuildForFixtureModelKeys(
    const std::vector<std::string> &modelKeys,
    const SymbolDefinitionSnapshot &snapshot, const BuildParams &params) {
  std::vector<Symbol2D> result;
  result.reserve(4);
  if (modelKeys.empty())
    return result;

  for (SymbolView view :
       {SymbolView::Front, SymbolView::Top, SymbolView::Bottom, SymbolView::Left}) {
    Symbol2D symbol;
    symbol.view = view;
    symbol.strokeWidthPx = params.previewStrokeWidthPx;

    for (const auto &[id, definition] : snapshot) {
      (void)id;
      if (!HasModelKey(modelKeys, definition.key.modelKey))
        continue;
      std::optional<SymbolView> defView = ToSymbolView(definition.key.viewKind);
      if (!defView || *defView != view)
        continue;

      for (const auto &command : definition.localCommands.commands) {
        std::visit(
            [&](const auto &c) {
              using T = std::decay_t<decltype(c)>;
              if constexpr (std::is_same_v<T, LineCommand>) {
                Polyline2D line{{c.x0, c.y0}, {c.x1, c.y1}};
                symbol.strokes.push_back(std::move(line));
              } else if constexpr (std::is_same_v<T, PolylineCommand>) {
                auto polyline = ToPolyline(c.points);
                if (polyline.size() >= 2)
                  symbol.strokes.push_back(std::move(polyline));
              } else if constexpr (std::is_same_v<T, PolygonCommand>) {
                auto polygon = ToPolyline(c.points);
                if (polygon.size() >= 3 && c.hasFill)
                  symbol.fill.push_back(PolygonWithHoles2D{std::move(polygon), {}});
              }
            },
            command);
      }
    }

    for (const auto &polygon : symbol.fill)
      for (const auto &point : polygon.outer)
        ExtendBounds(symbol.bounds, point);
    for (const auto &line : symbol.strokes)
      for (const auto &point : line)
        ExtendBounds(symbol.bounds, point);

    result.push_back(std::move(symbol));
  }

  return result;
}

} // namespace symbols
