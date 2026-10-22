#include "perastage_svg_symbol_builder.h"
#include "symbols/svg_symbol_presentation.h"

#include <algorithm>
#include <array>
#include <vector>

#include "opaque_pass_utils.h"
#include "symbols/fixture_symbol_availability.h"

namespace {
constexpr float kDefaultStrokeWidthMeters = RENDER_SCALE;

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
  poly.stroke.width = kDefaultStrokeWidthMeters;
  poly.fill.color = {fillRgb[0], fillRgb[1], fillRgb[2], 1.0f};
  poly.hasFill = true;

  buffer.commands.emplace_back(std::move(poly));
  // Keep fills in a dedicated source/layer so PDF replay renders fills
  // before strokes and preserves interior stroke details.
  buffer.metadata.push_back({false, true});
  buffer.sources.push_back("svg_fill");

  // Some rendering paths replay symbol command buffers instead of drawing SVG
  // polygons directly. Emit cutout polygons too so those paths preserve the
  // same interior openings parsed from the source SVG.
  for (const auto &hole : polygon.holes) {
    if (hole.size() < 3)
      continue;

    PolygonCommand holePoly{};
    holePoly.points.reserve(hole.size() * 2);
    for (const auto &point : hole) {
      holePoly.points.push_back(static_cast<float>(point.x));
      holePoly.points.push_back(static_cast<float>(point.y));
    }
    holePoly.stroke.color = {1.0f, 1.0f, 1.0f, 1.0f};
    holePoly.stroke.width = 0.0f;
    holePoly.fill.color = {1.0f, 1.0f, 1.0f, 1.0f};
    holePoly.hasFill = true;

    buffer.commands.emplace_back(std::move(holePoly));
    buffer.metadata.push_back({false, true});
    buffer.sources.push_back("svg_hole");
  }
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
  polyline.stroke.width = kDefaultStrokeWidthMeters;

  buffer.commands.emplace_back(std::move(polyline));
  buffer.metadata.push_back({true, false});
  buffer.sources.push_back("svg_stroke");
}
} // namespace

// Builds a canonical symbol definition from the resolved or fallback GDTF SVG view.
bool TryBuildPerastageSvgSymbolDefinition(const std::string &gdtfPath,
                                          SymbolViewKind viewKind,
                                          uint32_t symbolId,
                                          const std::array<float, 3> &fillRgb,
                                          SymbolDefinition &out) {
  const auto storedSvg =
      symbol_cache::LoadUsableFixtureSymbol(gdtfPath, viewKind);
  if (!storedSvg)
    return false;
  const PerastageSvgSymbolData &svg = *storedSvg;
  if (!svg.IsValid())
    return false;

  const auto anchor = symbols::SvgPresentationAnchor(svg);
  const double anchorX = anchor[0], anchorY = anchor[1];

  out = SymbolDefinition{};
  out.symbolId = symbolId;
  out.source = SymbolDefinition::Source::PerastageSvg;
  out.localCommands.currentSourceKey = "svg";

  symbols::VisitSvgPresentation(svg,
      [&](const auto &polygon) { AppendSvgPolygon(polygon, fillRgb, out.localCommands); },
      [&](const auto &line) { AppendSvgPolyline(line, out.localCommands); });

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
