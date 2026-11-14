#include "legend_composition_adapter.h"
#include "viewer2dcommandrenderer.h"

#include <algorithm>
#include <limits>

namespace viewer2d {
namespace {
using namespace layouts::composition;
int Rank(SymbolViewKind view) {
  switch (view) {
  case SymbolViewKind::Top: return 0;
  case SymbolViewKind::Bottom: return 1;
  case SymbolViewKind::Front: return 2;
  case SymbolViewKind::Left: return 3;
  case SymbolViewKind::Right: return 4;
  default: return 5;
  }
}
const SymbolDefinition *Find(const SymbolDefinitionSnapshot *snapshot,
    const std::string &key, SymbolViewKind view) {
  if (!snapshot) return nullptr;
  const SymbolDefinition *best = nullptr;
  for (const auto &[id, symbol] : *snapshot) {
    (void)id;
    if (symbol.key.modelKey != key) continue;
    if (symbol.key.viewKind == view) return &symbol;
    if (view == SymbolViewKind::Bottom &&
        (!best || Rank(symbol.key.viewKind) < Rank(best->key.viewKind))) best = &symbol;
  }
  return best;
}
Rect SvgBounds(const PerastageSvgSymbolData &svg) {
  double minX = std::numeric_limits<double>::infinity(), minY = minX;
  double maxX = -minX, maxY = -minX;
  auto include = [&](const PerastageSvgPoint &point) {
    const double x = point.x + svg.offsetXmm, y = point.y + svg.offsetYmm;
    minX = std::min(minX, x); minY = std::min(minY, y);
    maxX = std::max(maxX, x); maxY = std::max(maxY, y);
  };
  for (const auto &polygon : svg.fills) {
    for (const auto &point : polygon.points) include(point);
    for (const auto &hole : polygon.holes)
      for (const auto &point : hole) include(point);
  }
  for (const auto &line : svg.strokes)
    for (const auto &point : line.points) include(point);
  if (maxX <= minX || maxY <= minY) return {};
  return {minX, minY, maxX - minX, maxY - minY};
}
}

LegendCompositionResources ResolveLegendComposition(
    const std::vector<layouts::LegendResolvedItem> &items,
    const SymbolDefinitionSnapshot *snapshot, const LegendSvgResolver &resolveSvg) {
  LegendCompositionResources result;
  constexpr std::array<SymbolViewKind, 3> views = {
      SymbolViewKind::Bottom, SymbolViewKind::Front, SymbolViewKind::Right};
  for (const auto &item : items) {
    LegendCompositionRow row;
    row.id = item.typeName;
    row.name = item.displayName;
    row.count = std::to_string(item.count);
    row.channels = item.channelCount ? std::to_string(*item.channelCount) : "-";
    const std::array<bool, 3> enabled = {
        item.showBottomSymbol, item.showFrontSymbol, item.showSideSymbol};
    for (std::size_t view = 0; view < 3; ++view) {
      if (!enabled[view] || item.symbolKey.empty()) continue;
      ResolvedLegendSymbol symbol;
      symbol.svg = resolveSvg ? resolveSvg(item.symbolKey, item.gdtfPath, views[view]) : nullptr;
      symbol.commands = Find(snapshot, item.symbolKey, views[view]);
      auto &geometry = row.symbols[view];
      geometry.reference = result.symbols.size();
      if (symbol.svg) {
        geometry.kind = SymbolGeometryKind::Svg;
        geometry.bounds = SvgBounds(*symbol.svg);
      } else if (symbol.commands) {
        geometry.kind = SymbolGeometryKind::Commands;
        geometry.strokeScale = 1; // Captured stroke pixels become paper points.
        const auto &b = symbol.commands->bounds;
        geometry.bounds = {b.min.x, b.min.y, b.max.x - b.min.x, b.max.y - b.min.y};
      }
      result.symbols.push_back(symbol);
    }
    result.input.rows.push_back(std::move(row));
  }
  return result;
}
} // namespace viewer2d
