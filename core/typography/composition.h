#pragma once

#include "font_metrics.h"

namespace typography {
class FontCatalog;
enum class TextUnit { SceneMetres, PaperPoints };
struct TextStyle {
  double size = 0.12;
  TextUnit unit = TextUnit::SceneMetres;
  bool bold = false;
  double maxWidth = 0;
  int alignment = 1; // Left, center, right.
};
struct TextBounds {
  double left = 0, bottom = 0, right = 0, top = 0;
};
struct PositionedGlyph {
  std::string text;
  double x = 0, baseline = 0, advance = 0;
  std::string resourceId;
  uint32_t codepoint = 0;
  uint16_t glyphId = 0;
  size_t sourceBegin = 0, sourceEnd = 0;
  double offsetX = 0, offsetY = 0;
  uint32_t script = 0;
  bool rightToLeft = false;
  bool clusterContinuation = false;
  size_t styleIndex = 0;
  TextBounds ink; // Bounds relative to the positioned glyph origin.
};
struct ComposedLine {
  size_t first = 0, count = 0;
  double width = 0, baseline = 0;
  size_t sourceEnd = 0;
};
struct Composition {
  TextStyle style;
  std::vector<PositionedGlyph> glyphs;
  std::vector<ComposedLine> lines;
  std::vector<size_t> unsupportedBytes;
  std::string resourceId;
  FontIssue fontIssue = FontIssue::None;
  double ascent = 0, descent = 0, lineHeight = 0;
  TextBounds bounds; // Logical bounds relative to the first baseline.
};

Composition Compose(const std::string &text, const TextStyle &style,
                    const TtfFontMetrics &font,
                    const TtfFontMetrics *fallback = nullptr);
Composition Compose(const std::string &text, const TextStyle &style,
                    const FontCatalog &catalog, std::string_view family,
                    bool italic = false, FontUsage usage = FontUsage::Local);
} // namespace typography
