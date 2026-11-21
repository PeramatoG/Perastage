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
struct PositionedGlyph {
  std::string text;
  double x = 0, baseline = 0, advance = 0;
  std::string_view resourceId;
  uint32_t codepoint = 0;
  uint16_t glyphId = 0;
};
struct ComposedLine {
  size_t first = 0, count = 0;
  double width = 0, baseline = 0;
};
struct Composition {
  TextStyle style;
  std::vector<PositionedGlyph> glyphs;
  std::vector<ComposedLine> lines;
  std::vector<size_t> unsupportedBytes;
  std::string_view resourceId;
  FontIssue fontIssue = FontIssue::None;
  double ascent = 0, descent = 0, lineHeight = 0;
};

Composition Compose(const std::string &text, const TextStyle &style,
                    const TtfFontMetrics &font,
                    const TtfFontMetrics *fallback = nullptr);
Composition Compose(const std::string &text, const TextStyle &style,
                    const FontCatalog &catalog, std::string_view family,
                    bool italic = false);
} // namespace typography
