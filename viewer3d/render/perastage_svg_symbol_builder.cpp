#include "perastage_svg_symbol_builder.h"

#include <algorithm>
#include <array>
#include <vector>

#include "opaque_pass_utils.h"
#include "symbols/PerastageSvgSymbol.h"

namespace {
constexpr float kDefaultStrokeWidth = 1.0f;

void AppendSvgPolygon(const PerastageSvgPolygon &polygon,
                      const std::array<float, 3> &fillRgb,
                      CommandBuffer &buffer) {
  if (polygon.points.size() < 3)
    return;

  PolygonCommand poly{};
  poly.points.reserve(polygon.points.size() * 2);
  for (const auto &point : polygon.points) {
    poly.points.push_back(static_cast<float>(point.x));
    poly.points.push_back(static_cast<float>(point.y));
  }
  poly.stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
  poly.stroke.width = kDefaultStrokeWidth;
  poly.fill.color = {fillRgb[0], fillRgb[1], fillRgb[2], 1.0f};
  poly.hasFill = true;

  buffer.commands.emplace_back(std::move(poly));
  // Keep fills in a dedicated source/layer so PDF replay renders fills
  // before strokes and preserves interior stroke details.
  buffer.metadata.push_back({false, true});
  buffer.sources.push_back("svg_fill");
}

void AppendSvgPolyline(const PerastageSvgPolyline &line, CommandBuffer &buffer) {
  if (line.points.size() < 2)
    return;

  PolylineCommand polyline{};
  polyline.points.reserve(line.points.size() * 2);
  for (const auto &point : line.points) {
    polyline.points.push_back(static_cast<float>(point.x));
    polyline.points.push_back(static_cast<float>(point.y));
  }
  polyline.stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
  polyline.stroke.width = kDefaultStrokeWidth;

  buffer.commands.emplace_back(std::move(polyline));
  buffer.metadata.push_back({true, false});
  buffer.sources.push_back("svg_stroke");
}
} // namespace

bool TryBuildPerastageSvgSymbolDefinition(const std::string &gdtfPath,
                                          SymbolViewKind viewKind,
                                          uint32_t symbolId,
                                          const std::array<float, 3> &fillRgb,
                                          SymbolDefinition &out) {
  PerastageSvgSymbolData svg;
  if (!LoadPerastageSvgSymbolFromGdtf(gdtfPath, viewKind, svg))
    return false;
  if (!svg.IsValid())
    return false;

  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
  bool hasBounds = false;
  auto includePoint = [&](const PerastageSvgPoint &point) {
    const double px = point.x + svg.offsetXmm;
    const double py = point.y + svg.offsetYmm;
    if (!hasBounds) {
      minX = maxX = px;
      minY = maxY = py;
      hasBounds = true;
      return;
    }
    minX = std::min(minX, px);
    minY = std::min(minY, py);
    maxX = std::max(maxX, px);
    maxY = std::max(maxY, py);
  };
  for (const auto &polygon : svg.fills) {
    for (const auto &point : polygon.points)
      includePoint(point);
    for (const auto &hole : polygon.holes) {
      for (const auto &point : hole)
        includePoint(point);
    }
  }
  for (const auto &line : svg.strokes) {
    for (const auto &point : line.points)
      includePoint(point);
  }
  const double anchorX = hasBounds ? (minX + maxX) * 0.5 : 0.0;
  const double anchorY = hasBounds ? (minY + maxY) * 0.5 : 0.0;

  out = SymbolDefinition{};
  out.symbolId = symbolId;
  out.localCommands.currentSourceKey = "svg";

  for (const auto &polygon : svg.fills)
    AppendSvgPolygon(polygon, fillRgb, out.localCommands);
  for (const auto &line : svg.strokes)
    AppendSvgPolyline(line, out.localCommands);

  if (out.localCommands.commands.empty())
    return false;

  for (auto &cmd : out.localCommands.commands) {
    if (auto *polygon = std::get_if<PolygonCommand>(&cmd)) {
      for (size_t i = 0; i + 1 < polygon->points.size(); i += 2) {
        polygon->points[i] =
            (polygon->points[i] + static_cast<float>(svg.offsetXmm - anchorX)) *
            RENDER_SCALE;
        polygon->points[i + 1] =
            (polygon->points[i + 1] +
             static_cast<float>(svg.offsetYmm - anchorY)) *
            RENDER_SCALE;
      }
    } else if (auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      for (size_t i = 0; i + 1 < polyline->points.size(); i += 2) {
        polyline->points[i] =
            (polyline->points[i] + static_cast<float>(svg.offsetXmm - anchorX)) *
            RENDER_SCALE;
        polyline->points[i + 1] =
            (polyline->points[i + 1] +
             static_cast<float>(svg.offsetYmm - anchorY)) *
            RENDER_SCALE;
      }
    }
  }

  out.bounds = ComputeSymbolBounds(out.localCommands);
  return true;
}
