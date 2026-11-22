#include "text_block.h"
#include "font_resources.h"
#include "unicode_shaping.h"
#include <algorithm>
#include <cmath>

namespace typography {
namespace {
FontLineMetrics Metrics(const TtfFontMetrics *font, double size) {
  return font ? MeasureFontLine(*font, size) : FontLineMetrics{0, 0, size};
}

bool IsSpace(const PositionedGlyph &glyph) {
  return glyph.codepoint == ' ' || glyph.codepoint == '\t';
}

std::vector<ShapingStyle> SliceStyles(const std::vector<ShapingStyle> &styles,
                                     size_t start, size_t end) {
  std::vector<ShapingStyle> slice;
  auto first = std::upper_bound(styles.begin(), styles.end(), start,
      [](size_t offset, const ShapingStyle &style) { return offset < style.start; });
  if (first != styles.begin()) --first;
  for (; first != styles.end() && first->start < end; ++first) {
    slice.push_back(*first);
    slice.back().start = first->start > start ? first->start - start : 0;
  }
  return slice;
}
}

TextBlockPlan ComposeTextBlock(const TextBlock &block, double width, double height,
                              const FontCatalog &catalog) {
  TextBlockPlan plan;
  constexpr double padding = 4;
  if (width <= 0 || height <= 0) return plan;
  if (block.containsUnsupportedObjects)
    plan.diagnostics.insert("Some rich-text objects cannot be rendered. The original data is retained.");
  const double defaultSize = std::clamp(block.fontSize, 1, 1024);
  const auto defaultFace = catalog.Resolve({}, block.bold, block.italic).face;
  const auto defaultMetrics = Metrics(defaultFace ? &defaultFace->metrics : nullptr, defaultSize);
  double top = padding;
  for (const auto &paragraph : block.lines) {
    std::string text;
    std::vector<ShapingStyle> styles;
    for (size_t i = 0; i < paragraph.runs.size(); ++i) {
      const auto &run = paragraph.runs[i];
      if (run.text.empty()) continue;
      const double size = run.fontSize > 0 ? std::min(run.fontSize, 1024) : defaultSize;
      const auto resolved = catalog.Resolve(run.family, run.bold, run.italic);
      if (resolved.issue != FontIssue::None)
        plan.diagnostics.insert(DescribeFontIssue(catalog, run.family, run.bold, resolved.issue, run.italic));
      styles.push_back({text.size(), i, size, CompositionFonts(catalog, resolved.face, run.bold, run.italic)});
      text += run.text;
    }
    auto shaped = ShapeStyledLine(text, styles);
    if (!shaped.unsupportedBytes.empty())
      plan.diagnostics.insert("Some text characters are unavailable in the approved fonts.");
    const auto left = padding + paragraph.leftIndent;
    const auto right = width - padding - paragraph.rightIndent;
    const auto widthFor = [&](size_t start) {
      return std::max(0.0, right - left - (start == 0 ? paragraph.firstLineIndent : 0));
    };
    const auto breaks = WrapShapedLine(text, shaped, block.wrap ? widthFor(0) : -1, widthFor(1));
    top += paragraph.spaceBefore;
    size_t start = 0;
    for (const auto end : breaks) {
      auto line = breaks.size() == 1 ? std::move(shaped)
          : ShapeStyledLine(std::string_view(text).substr(start, end - start),
                            SliceStyles(styles, start, end), text, start);
      auto ascent = defaultMetrics.ascent, descent = defaultMetrics.descent, lineHeight = defaultMetrics.lineHeight;
      for (const auto &glyph : line.glyphs) {
        const auto &run = paragraph.runs[glyph.styleIndex];
        const double size = run.fontSize > 0 ? std::min(run.fontSize, 1024) : defaultSize;
        const auto *face = catalog.FindResource(glyph.resourceId);
        const auto metrics = Metrics(face ? &face->metrics : nullptr, size);
        ascent = std::max({ascent, metrics.ascent, glyph.offsetY + glyph.ink.top});
        descent = std::max({descent, metrics.descent, -glyph.offsetY - glyph.ink.bottom});
        lineHeight = std::max(lineHeight, metrics.lineHeight);
      }
      if (top >= height - padding) return plan;
      const auto baseline = top + ascent;
      const auto available = widthFor(start);
      const auto alignment = paragraph.alignment.value_or(block.alignment);
      double origin = left + (start == 0 ? paragraph.firstLineIndent : 0), extra = 0;
      if (alignment == TextBlock::Alignment::Center) origin += (available - line.width) / 2;
      if (alignment == TextBlock::Alignment::Right) origin += available - line.width;
      if (alignment == TextBlock::Alignment::Justified && end < text.size()) {
        const auto spaces = std::count_if(line.glyphs.begin(), line.glyphs.end(), IsSpace);
        if (spaces) extra = std::max(0.0, available - line.width) / spaces;
      }
      double adjustment = 0;
      for (auto &glyph : line.glyphs) {
        const auto &run = paragraph.runs[glyph.styleIndex];
        const double size = run.fontSize > 0 ? std::min(run.fontSize, 1024) : defaultSize;
        const double x = origin + glyph.x + adjustment;
        if (glyph.advance > 0 && run.underline)
          plan.decorations.push_back({x, baseline + size * 0.08, glyph.advance, size * 0.05, run.color});
        if (glyph.advance > 0 && run.strikethrough)
          plan.decorations.push_back({x, baseline - size * 0.35, glyph.advance, size * 0.05, run.color});
        if (IsSpace(glyph)) adjustment += extra;
        const auto y = baseline - glyph.offsetY;
        glyph.sourceBegin += start;
        glyph.sourceEnd += start;
        plan.glyphs.push_back({std::move(glyph), x, y, size, run.color});
      }
      top += std::max(lineHeight, ascent + descent) * std::clamp(paragraph.lineSpacing, 0.5, 5.0);
      start = end;
    }
    top += paragraph.spaceAfter;
  }
  return plan;
}
}
