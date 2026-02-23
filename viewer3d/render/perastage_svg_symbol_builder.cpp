#include "perastage_svg_symbol_builder.h"

#include <vector>

#include "opaque_pass_utils.h"
#include "symbols/PerastageSvgSymbol.h"

namespace {
constexpr float kDefaultStrokeWidthMeters = 0.001f;
constexpr float kMillimetersToMeters = 0.001f;

void AppendSvgPolygon(const PerastageSvgPolygon &polygon,
                      const std::array<float, 3> &fillColor,
                      CommandBuffer &buffer) {
  if (polygon.points.size() < 3)
    return;

  PolygonCommand poly{};
  poly.points.reserve(polygon.points.size() * 2);
  for (const auto &point : polygon.points) {
    poly.points.push_back(static_cast<float>(point.x) * kMillimetersToMeters);
    poly.points.push_back(static_cast<float>(point.y) * kMillimetersToMeters);
  }
  poly.stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
  poly.stroke.width = kDefaultStrokeWidthMeters;
  poly.fill.color = {fillColor[0], fillColor[1], fillColor[2], 1.0f};
  poly.hasFill = true;

  buffer.commands.emplace_back(std::move(poly));
  buffer.metadata.push_back({true, true});
  buffer.sources.push_back("svg");
}

void AppendSvgPolyline(const PerastageSvgPolyline &line, CommandBuffer &buffer) {
  if (line.points.size() < 2)
    return;

  PolylineCommand polyline{};
  polyline.points.reserve(line.points.size() * 2);
  for (const auto &point : line.points) {
    polyline.points.push_back(static_cast<float>(point.x) * kMillimetersToMeters);
    polyline.points.push_back(static_cast<float>(point.y) * kMillimetersToMeters);
  }
  polyline.stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
  polyline.stroke.width = kDefaultStrokeWidthMeters;

  buffer.commands.emplace_back(std::move(polyline));
  buffer.metadata.push_back({true, false});
  buffer.sources.push_back("svg");
}
} // namespace

bool TryBuildPerastageSvgSymbolDefinition(const std::string &gdtfPath,
                                          SymbolViewKind viewKind,
                                          uint32_t symbolId,
                                          const std::array<float, 3> &fillColor,
                                          SymbolDefinition &out) {
  PerastageSvgSymbolData svg;
  if (!LoadPerastageSvgSymbolFromGdtf(gdtfPath, viewKind, svg))
    return false;
  if (!svg.IsValid())
    return false;

  out = SymbolDefinition{};
  out.symbolId = symbolId;
  out.localCommands.currentSourceKey = "svg";

  for (const auto &polygon : svg.fills)
    AppendSvgPolygon(polygon, fillColor, out.localCommands);
  for (const auto &line : svg.strokes)
    AppendSvgPolyline(line, out.localCommands);

  if (out.localCommands.commands.empty())
    return false;

  for (auto &cmd : out.localCommands.commands) {
    if (auto *polygon = std::get_if<PolygonCommand>(&cmd)) {
      for (size_t i = 0; i + 1 < polygon->points.size(); i += 2) {
        polygon->points[i] += static_cast<float>(svg.offsetXmm) * kMillimetersToMeters;
        polygon->points[i + 1] += static_cast<float>(svg.offsetYmm) *
                                  kMillimetersToMeters;
      }
    } else if (auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      for (size_t i = 0; i + 1 < polyline->points.size(); i += 2) {
        polyline->points[i] += static_cast<float>(svg.offsetXmm) * kMillimetersToMeters;
        polyline->points[i + 1] += static_cast<float>(svg.offsetYmm) *
                                   kMillimetersToMeters;
      }
    }
  }

  out.bounds = ComputeSymbolBounds(out.localCommands);
  return true;
}
