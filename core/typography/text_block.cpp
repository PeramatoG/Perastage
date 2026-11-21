#include "text_block.h"
#include "font_resources.h"
#include <algorithm>
#include <cmath>

namespace typography {
namespace {
struct RunGlyph {
  PositionedGlyph glyph;
  const TextBlock::Run *style;
  double ascent, descent, height, size;
};

bool IsBreak(const RunGlyph &glyph) {
  return glyph.glyph.codepoint == ' ' || glyph.glyph.codepoint == '\t';
}

void AddGlyph(TextBlockPlan &plan, RunGlyph &entry, double x, double baseline) {
  const auto &style = *entry.style;
  if (entry.glyph.advance > 0 && style.underline)
    plan.decorations.push_back({x, baseline + entry.size * 0.08, entry.glyph.advance,
                               entry.size * 0.05, style.color});
  if (entry.glyph.advance > 0 && style.strikethrough)
    plan.decorations.push_back({x, baseline - entry.ascent * 0.35, entry.glyph.advance,
                               entry.size * 0.05, style.color});
  plan.glyphs.push_back({std::move(entry.glyph), x, baseline, entry.size, style.color});
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
  const auto empty = Compose("", {defaultSize, TextUnit::PaperPoints, block.bold}, catalog, {});
  double top = padding;
  for (const auto &paragraph : block.lines) {
    std::vector<RunGlyph> glyphs;
    for (const auto &run : paragraph.runs) {
      const double size = run.fontSize > 0 ? std::min(run.fontSize, 1024) : defaultSize;
      auto composed = Compose(run.text, {size, TextUnit::PaperPoints, run.bold, 0, 0},
                              catalog, run.family, run.italic);
      if (composed.fontIssue != FontIssue::None)
        plan.diagnostics.insert(DescribeFontIssue(catalog, run.family, run.bold, composed.fontIssue, run.italic));
      if (!composed.unsupportedBytes.empty())
        plan.diagnostics.insert("Some text characters are unavailable in the approved fonts.");
      for (auto &glyph : composed.glyphs)
        glyphs.push_back({std::move(glyph), &run, composed.ascent,
                          composed.descent, composed.lineHeight, size});
    }

    top += paragraph.spaceBefore;
    size_t start = 0;
    do {
      const auto left = padding + paragraph.leftIndent + (start == 0 ? paragraph.firstLineIndent : 0);
      const auto right = width - padding - paragraph.rightIndent;
      const auto available = std::max(0.0, right - left);
      double used = 0, atBreak = 0;
      size_t end = start, wrapAt = start;
      for (; end < glyphs.size(); ++end) {
        const auto advance = glyphs[end].glyph.advance;
        if (block.wrap && end > start && used + advance > available) break;
        used += advance;
        if (IsBreak(glyphs[end])) { wrapAt = end + 1; atBreak = used; }
      }
      if (end < glyphs.size() && wrapAt > start) { end = wrapAt; used = atBreak; }
      double ascent = empty.ascent, descent = empty.descent, lineHeight = empty.lineHeight;
      for (size_t i = start; i < end; ++i) {
        ascent = std::max(ascent, glyphs[i].ascent);
        descent = std::max(descent, glyphs[i].descent);
        lineHeight = std::max(lineHeight, glyphs[i].height);
      }
      const auto baseline = top + ascent;
      if (top >= height - padding) return plan;
      const auto alignment = paragraph.alignment.value_or(block.alignment);
      double x = left, extra = 0;
      if (alignment == TextBlock::Alignment::Center) x += (available - used) / 2;
      if (alignment == TextBlock::Alignment::Right) x += available - used;
      if (alignment == TextBlock::Alignment::Justified && end < glyphs.size()) {
        const auto spaces = std::count_if(glyphs.begin() + start, glyphs.begin() + end, IsBreak);
        if (spaces) extra = std::max(0.0, available - used) / spaces;
      }
      for (size_t i = start; i < end; ++i) {
        const auto advance = glyphs[i].glyph.advance + (IsBreak(glyphs[i]) ? extra : 0);
        AddGlyph(plan, glyphs[i], x, baseline);
        x += advance;
      }
      top += std::max(lineHeight, ascent + descent) * std::clamp(paragraph.lineSpacing, 0.5, 5.0);
      start = end;
    } while (start < glyphs.size());
    top += paragraph.spaceAfter;
  }
  return plan;
}
}
