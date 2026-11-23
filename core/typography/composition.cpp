#include "composition.h"
#include "font_resources.h"
#include "unicode_shaping.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace typography {
std::vector<size_t> WrapShapedLine(std::string_view text, const ShapedLine &shaped, double maxWidth, double continuationWidth) {
  if (maxWidth < 0 || shaped.width <= maxWidth) return {text.size()};
  std::map<size_t, std::pair<size_t, double>> clusters;
  for (const auto &glyph : shaped.glyphs) {
    auto &cluster = clusters[glyph.sourceBegin];
    cluster.first = glyph.sourceEnd;
    cluster.second += glyph.advance;
  }
  const auto breaks = UnicodeLineBreaks(text);
  std::vector<size_t> result;
  auto begin = clusters.begin();
  while (begin != clusters.end()) {
    double width = 0;
    auto end = begin, preferred = begin;
    for (; end != clusters.end(); ++end) {
      if (end != begin && width + end->second.second > maxWidth) break;
      width += end->second.second;
      if (std::binary_search(breaks.begin(), breaks.end(), end->second.first))
        preferred = std::next(end);
    }
    if (end != clusters.end() && preferred != begin) end = preferred;
    result.push_back(end == clusters.end() ? text.size() : end->first);
    begin = end;
    if (continuationWidth >= 0) maxWidth = continuationWidth;
  }
  if (result.empty()) result.push_back(text.size());
  return result;
}

namespace {
Composition ComposeFonts(const std::string &text, const TextStyle &style,
                         std::span<const TtfFontMetrics *const> fonts) {
  Composition out;
  out.style = style;
  if (fonts.empty() || !fonts.front()->valid || !std::isfinite(style.size) || style.size <= 0)
    return out;
  const auto &font = *fonts.front();
  out.resourceId = font.resourceId;
  const auto [primaryAscent, primaryDescent, primaryHeight] = MeasureFontLine(font, style.size);
  out.ascent = primaryAscent;
  out.descent = primaryDescent;
  out.lineHeight = primaryHeight;
  double baseline = 0;
  const auto emit = [&](ShapedLine shaped, size_t sourceOffset, size_t sourceEnd) {
    auto ascent = primaryAscent, descent = primaryDescent, height = primaryHeight;
    for (const auto &glyph : shaped.glyphs) {
      if (glyph.resourceId == font.resourceId) continue;
      const auto found = std::find_if(fonts.begin(), fonts.end(),
          [&](const auto *face) { return face->resourceId == glyph.resourceId; });
      if (found == fonts.end()) continue;
      const auto metrics = MeasureFontLine(**found, style.size);
      ascent = std::max(ascent, metrics.ascent);
      descent = std::max(descent, metrics.descent);
      height = std::max(height, metrics.lineHeight);
    }
    out.ascent = std::max(out.ascent, ascent);
    out.descent = std::max(out.descent, descent);
    out.lineHeight = std::max({out.lineHeight, height, ascent + descent});
    const auto width = shaped.width;
    const double shift = style.alignment == 1 ? -width / 2 : style.alignment == 2 ? -width : 0;
    out.lines.push_back({out.glyphs.size(), shaped.glyphs.size(), width, baseline, sourceEnd});
    TextBounds bounds{shift, baseline - descent, shift + width, baseline + ascent};
    for (auto &g : shaped.glyphs) {
      g.x += shift;
      g.baseline += baseline;
      bounds.left = std::min(bounds.left, g.x + g.ink.left);
      bounds.right = std::max(bounds.right, g.x + g.ink.right);
      bounds.bottom = std::min(bounds.bottom, g.baseline + g.ink.bottom);
      bounds.top = std::max(bounds.top, g.baseline + g.ink.top);
      g.sourceBegin += sourceOffset;
      g.sourceEnd += sourceOffset;
      out.glyphs.push_back(std::move(g));
    }
    for (const auto byte : shaped.unsupportedBytes)
      out.unsupportedBytes.push_back(sourceOffset + byte);
    if (out.lines.size() == 1) out.bounds = bounds;
    else {
      out.bounds.left = std::min(out.bounds.left, bounds.left);
      out.bounds.right = std::max(out.bounds.right, bounds.right);
      out.bounds.bottom = std::min(out.bounds.bottom, bounds.bottom);
      out.bounds.top = std::max(out.bounds.top, bounds.top);
    }
    baseline -= std::max(height, ascent + descent);
  };

  size_t start = 0;
  while (true) {
    const auto newline = text.find_first_of("\r\n", start);
    const std::string_view paragraph = std::string_view(text).substr(start, newline - start);
    auto shaped = ShapeUnicodeLine(paragraph, style.size, fonts);
    const auto breaks = WrapShapedLine(paragraph, shaped, style.maxWidth > 0 ? style.maxWidth : -1);
    if (breaks.size() == 1) emit(std::move(shaped), start, start + paragraph.size());
    else {
      size_t offset = 0;
      for (const auto end : breaks) {
        emit(ShapeUnicodeLine(paragraph.substr(offset, end - offset), style.size, fonts,
                             paragraph, offset), start + offset, start + end);
        offset = end;
      }
    }
    if (newline == std::string::npos) break;
    start = newline + 1;
    if (text[newline] == '\r' && start < text.size() && text[start] == '\n') ++start;
  }
  return out;
}
}

Composition Compose(const std::string &text, const TextStyle &style,
                    const TtfFontMetrics &font, const TtfFontMetrics *fallback) {
  const std::array fonts{&font, fallback};
  return ComposeFonts(text, style, std::span(fonts).first(fallback && fallback->valid ? 2 : 1));
}

Composition Compose(const std::string &text, const TextStyle &style,
                    const FontCatalog &catalog, std::string_view family,
                    bool italic, FontUsage usage) {
  const auto requested = catalog.Resolve(family, style.bold, italic, usage);
  const auto fonts = CompositionFonts(catalog, requested.face.get(), style.bold, italic);
  auto result = ComposeFonts(text, style, fonts);
  result.fontIssue = requested.issue;
  return result;
}
std::vector<const TtfFontMetrics *> CompositionFonts(const FontCatalog &catalog,
                                                     const FontResource *face, bool bold, bool italic) {
  std::vector<const TtfFontMetrics *> fonts;
  if (face) fonts.push_back(&face->metrics);
  for (const auto candidate : {kDefaultFamily, std::string_view("Noto Sans Arabic"),
                               std::string_view("Noto Sans Hebrew")}) {
    const auto fallback = catalog.Resolve(candidate, bold, candidate == kDefaultFamily && italic);
    if (fallback.face && std::find(fonts.begin(), fonts.end(), &fallback.face->metrics) == fonts.end())
      fonts.push_back(&fallback.face->metrics);
  }
  return fonts;
}
}
