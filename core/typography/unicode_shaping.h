#pragma once
#include "composition.h"
#include <span>

namespace typography {
struct FontResource;
std::vector<const TtfFontMetrics *> CompositionFonts(const FontCatalog &, const FontResource *,
                                                     bool bold, bool italic);
struct ShapedLine {
  std::vector<PositionedGlyph> glyphs;
  std::vector<size_t> unsupportedBytes;
  double width = 0;
};

ShapedLine ShapeUnicodeLine(std::string_view text, double size,
                            std::span<const TtfFontMetrics *const> fonts,
                            std::string_view paragraph = {}, size_t paragraphOffset = 0);
struct ShapingStyle {
  size_t start = 0, index = 0;
  double size = 0;
  std::vector<const TtfFontMetrics *> fonts;
};
ShapedLine ShapeStyledLine(std::string_view text, std::span<const ShapingStyle> styles,
                           std::string_view paragraph = {}, size_t paragraphOffset = 0);
std::vector<size_t> WrapShapedLine(std::string_view text, const ShapedLine &line, double width, double continuationWidth = -1);
bool IsRightToLeftScalar(uint32_t codepoint);
std::u16string DecodeUtf16(std::string_view text);
std::vector<size_t> UnicodeLineBreaks(std::string_view text);
}
