#pragma once
#include "font_metrics.h"
#include <memory>
#include <vector>

namespace typography {
// Font contours in em units, with an upward Y axis and the original baseline.
struct GlyphPathElement {
  enum class Kind { Move, Line, Quadratic, Close } kind;
  double x = 0, y = 0, cx = 0, cy = 0;
  bool hole = false; // Used by Close; TrueType counters retain their topology.
};
struct GlyphOutline {
  std::vector<GlyphPathElement> path;
};
// Reads the bundled TrueType fonts, including transformed composite accents.
std::shared_ptr<const GlyphOutline> ReadGlyphOutline(const TtfFontMetrics &font,
                                                    unsigned char winAnsi);
} // namespace typography
