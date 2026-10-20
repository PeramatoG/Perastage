#pragma once
#include "font_metrics.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace typography {
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
  double ascent = 0, descent = 0, lineHeight = 0;
};

// Strict UTF-8 decoding and a bounded WinAnsi repertoire shared with PDF.
// Unsupported source bytes remain in the source; callers must display diagnostics.
inline Composition Compose(const std::string &text, const TextStyle &style,
                           const TtfFontMetrics &font) {
  if (!font.valid || font.unitsPerEm <= 0 || !std::isfinite(style.size) || style.size <= 0)
    throw std::runtime_error("Typography font or size is invalid");
  Composition out;
  out.style = style;
  const double scale = style.size / font.unitsPerEm;
  out.ascent = font.ascent * scale;
  out.descent = std::abs(font.descent) * scale;
  out.lineHeight = (font.ascent + std::abs(font.descent) + font.lineGap) * scale;
  size_t first = 0;
  double width = 0, baseline = 0;
  auto finish = [&] {
    const double shift = style.alignment == 1 ? -width / 2 : style.alignment == 2 ? -width : 0;
    for (size_t j = first; j < out.glyphs.size(); ++j) out.glyphs[j].x += shift;
    out.lines.push_back({first, out.glyphs.size() - first, width, baseline});
    first = out.glyphs.size(); width = 0; baseline -= out.lineHeight;
  };
  for (size_t i = 0; i < text.size();) {
    const size_t start = i;
    const auto lead = static_cast<unsigned char>(text[i++]);
    uint32_t cp = lead;
    int more = lead < 128 ? 0 : lead >= 0xc2 && lead <= 0xdf ? 1 :
               lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : -1;
    bool valid = more >= 0;
    if (more > 0) {
      cp = lead & ((1u << (6-more))-1);
      for (int j = 0; j < more; ++j) {
        if (i >= text.size() || (static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) { valid = false; break; }
        cp = (cp << 6) | (static_cast<unsigned char>(text[i++]) & 0x3f);
      }
      valid = valid && cp >= (more == 1 ? 0x80u : more == 2 ? 0x800u : 0x10000u) &&
              cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
    }
    if (cp == '\n') { finish(); continue; }
    if (cp == '\r') continue;
    const std::string glyph = text.substr(start, i-start);
    const std::string encoded = EncodeWinAnsi(glyph);
    if (!valid || cp < 32 || (cp >= 127 && cp < 160) ||
        (encoded == "?" && cp != '?')) { out.unsupportedBytes.push_back(start); continue; }
    const double advance = font.advanceWidths[static_cast<unsigned char>(encoded[0])] * scale;
    if (style.maxWidth > 0 && width > 0 && width + advance > style.maxWidth) finish();
    out.glyphs.push_back({glyph, width, baseline, advance});
    width += advance;
  }
  finish();
  return out;
}
} // namespace typography
