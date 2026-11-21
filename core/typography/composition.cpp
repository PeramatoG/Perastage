#include "composition.h"
#include "font_resources.h"
#include <algorithm>
#include <cmath>

namespace typography {
namespace {
uint32_t NextScalar(std::string_view text, size_t &index) {
  const auto lead = static_cast<unsigned char>(text[index++]);
  if (lead < 0x80) return lead;
  if (lead < 0xc2 || lead > 0xf4) return 0xfffd;
  const unsigned extra = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
  uint32_t cp = lead & ((1u << (6 - extra)) - 1);
  for (unsigned i = 0; i < extra; ++i) {
    if (index == text.size()) return 0xfffd;
    const auto byte = static_cast<unsigned char>(text[index]);
    if ((byte & 0xc0) != 0x80) return 0xfffd;
    ++index;
    cp = (cp << 6) | (byte & 0x3f);
  }
  if (cp < (extra == 1 ? 0x80u : extra == 2 ? 0x800u : 0x10000u) ||
      cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return 0xfffd;
  return cp;
}

// This compositor covers independently positioned left-to-right glyphs.
bool NeedsShaping(uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036f) || (cp >= 0x0590 && cp <= 0x109f) ||
         (cp >= 0x1780 && cp <= 0x17ff) || (cp >= 0x1ab0 && cp <= 0x1aff) ||
         (cp >= 0x1dc0 && cp <= 0x1dff) || (cp >= 0x200b && cp <= 0x200f) ||
         (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069) ||
         (cp >= 0x20d0 && cp <= 0x20ff) || (cp >= 0xfe00 && cp <= 0xfe0f) ||
         (cp >= 0xfe20 && cp <= 0xfe2f);
}

}

Composition Compose(const std::string &text, const TextStyle &style,
                    const TtfFontMetrics &font, const TtfFontMetrics *fallback) {
  Composition out;
  out.style = style;
  out.resourceId = font.resourceId;
  if (!font.valid || font.glyphAdvances.empty() || !std::isfinite(style.size) || style.size <= 0)
    return out;
  const auto scale = style.size / font.unitsPerEm;
  out.ascent = font.ascent * scale;
  out.descent = std::abs(font.descent) * scale;
  out.lineHeight = (font.ascent + std::abs(font.descent) + font.lineGap) * scale;
  size_t first = 0;
  double width = 0, baseline = 0;
  const auto finish = [&] {
    const double shift = style.alignment == 1 ? -width / 2 : style.alignment == 2 ? -width : 0;
    for (size_t i = first; i < out.glyphs.size(); ++i) out.glyphs[i].x += shift;
    out.lines.push_back({first, out.glyphs.size() - first, width, baseline});
    first = out.glyphs.size();
    width = 0;
    baseline -= out.lineHeight;
  };
  for (size_t i = 0; i < text.size();) {
    const auto start = i;
    const auto cp = NextScalar(text, i);
    if (cp == '\n') { finish(); continue; }
    if (cp == '\r') continue;
    const auto scalar = cp == '\t' ? uint32_t(' ') : cp;
    const bool supported = !NeedsShaping(cp) && (cp >= 32 || cp == '\t');
    uint16_t glyph = supported ? font.GlyphFor(scalar) : 0;
    const auto *face = &font;
    if (!glyph && supported && fallback && (glyph = fallback->GlyphFor(scalar))) face = fallback;
    if (!glyph || (cp == 0xfffd && text.compare(start, i - start, "\xef\xbf\xbd") != 0))
      out.unsupportedBytes.push_back(start);
    const auto advance = face->glyphAdvances[glyph] * style.size / face->unitsPerEm;
    if (style.maxWidth > 0 && width > 0 && width + advance > style.maxWidth) finish();
    out.glyphs.push_back({text.substr(start, i - start), width, baseline, advance,
                          face->resourceId, cp, glyph});
    width += advance;
  }
  finish();
  return out;
}

Composition Compose(const std::string &text, const TextStyle &style,
                    const FontCatalog &catalog, std::string_view family, bool italic) {
  const auto requested = catalog.Resolve(family, style.bold, italic);
  const auto fallback = catalog.Resolve(kDefaultFamily, style.bold, italic);
  Composition result;
  result.style = style;
  if (requested.face)
    result = Compose(text, style, requested.face->metrics,
                     fallback.face ? &fallback.face->metrics : nullptr);
  result.fontIssue = requested.issue;
  return result;
}
} // namespace typography
