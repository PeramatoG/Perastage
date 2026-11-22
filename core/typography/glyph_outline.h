#pragma once
#include "font_metrics.h"
#include <memory>
#include <string_view>
#include <vector>

namespace typography {
// Font contours in em units, with an upward Y axis and the original baseline.
struct GlyphPathElement {
  enum class Kind { Move, Line, Quadratic, Cubic, Close } kind;
  double x = 0, y = 0, cx = 0, cy = 0;
  bool hole = false; // Used by Close; TrueType counters retain their topology.
  double dx = 0, dy = 0; // Second cubic control point.
};
struct GlyphOutline {
  std::vector<GlyphPathElement> path;
  bool substituted = false;
};
class FontCatalog;
std::shared_ptr<const GlyphOutline> ReadGlyphOutlineById(const TtfFontMetrics &font,
                                                        uint16_t glyph);
std::shared_ptr<const GlyphOutline> ResolveGlyphOutline(const FontCatalog &catalog,
                                                       std::string_view resource,
                                                       uint16_t glyph);
} // namespace typography
