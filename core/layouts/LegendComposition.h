#pragma once

#include "DocumentComposition.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace layouts::composition {

enum class LegendView : std::size_t { Bottom, Front, Side };
enum class SymbolGeometryKind { Missing, Svg, Commands };
struct LegendSymbolGeometry {
  SymbolGeometryKind kind = SymbolGeometryKind::Missing;
  std::size_t reference = 0; // Caller-owned immutable resolved view, not SVG bytes.
  Rect bounds;
};
struct LegendCompositionRow {
  std::string id;
  std::string count;
  std::string name;
  std::string channels = "-";
  bool visible = true;
  std::array<LegendSymbolGeometry, 3> symbols;
};

// Bounded compatibility presets, not persistent style settings. Both use the
// same composition algorithm. Font sources remain wx platform fonts and PDF's
// legacy TTF/Type1 fonts, respectively; family names do not establish parity.
enum class LegacyLegendMetrics { Preview, VectorExport };
struct LegacyLegendStyle {
  LegacyLegendMetrics metrics = LegacyLegendMetrics::Preview;
  double typographyScale = 1;
  double columnGap = 6;
  double separatorGap = 2;
};
struct LegendCompositionInput {
  double width = 0;
  double height = 0;
  bool showChannelColumn = true;
  std::vector<LegendCompositionRow> rows;
  LegacyLegendStyle style;
};
struct LegendColumn {
  double x = 0;
  double width = 0;
};
struct LegendTextLine {
  std::string text; // Original UTF-8, even if a backend cannot encode it.
  TextRole role = TextRole::Normal;
  Point top;
  Point baseline;
  double width = 0;
  bool overflow = false;
};
struct LegendSymbolPlacement {
  std::size_t row = 0;
  LegendView view = LegendView::Bottom;
  LegendSymbolGeometry geometry;
  Rect slot;
  Rect draw;
  double scale = 0;
  // Commands retain the historical origin anchor; SVG is bounds-aligned.
  double commandOriginX = 0;
};
struct LegendRowPlan {
  std::size_t sourceRow = 0;
  std::string id;
  Rect frame;
  std::array<std::string, 2> nameLines;
  bool overflow = false;
};
struct LegendCompositionPlan {
  Rect clip;
  bool whiteBackground = true;
  double borderWidth = 0.5;
  double fontSize = 0;
  double rowHeight = 0;
  std::array<LegendColumn, 3> symbolColumns;
  LegendColumn countColumn, typeColumn, channelColumn;
  Point separatorStart, separatorEnd;
  std::vector<LegendRowPlan> rows;
  std::vector<LegendTextLine> text;
  std::vector<LegendSymbolPlacement> symbols;
  std::size_t hiddenRows = 0;
  std::size_t clippedRows = 0;
  bool overflow = false;
};

LegendCompositionPlan ComposeLegend(const LegendCompositionInput &input,
                                    const TextMetrics &metrics);

} // namespace layouts::composition
