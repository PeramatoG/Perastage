#include "unicode_shaping.h"
#include <hb-ot.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

namespace typography {
namespace {
struct UnicodeLine {
  std::u16string utf16;
  std::vector<size_t> bytes{0};
  std::vector<size_t> invalid;
  explicit UnicodeLine(std::string_view text, bool expandTab = true) {
    const auto *data = reinterpret_cast<const uint8_t *>(text.data());
    const auto length = static_cast<int32_t>(text.size());
    for (int32_t pos = 0; pos < length;) {
      const auto begin = pos;
      UChar32 cp;
      U8_NEXT(data, pos, length, cp);
      if (cp < 0) { invalid.push_back(begin); cp = 0xfffd; }
      if (expandTab && cp == '\t') cp = ' ';
      if (cp > 0xffff) {
        utf16.push_back(U16_LEAD(cp));
        bytes.push_back(begin);
        utf16.push_back(U16_TRAIL(cp));
      } else {
        utf16.push_back(static_cast<char16_t>(cp));
      }
      bytes.push_back(pos);
    }
  }
};

using HbFont = std::shared_ptr<hb_font_t>;
HbFont MakeFont(const TtfFontMetrics &font, hb_memory_mode_t mode) {
  std::unique_ptr<hb_blob_t, decltype(&hb_blob_destroy)> blob(
      hb_blob_create(font.data.data(), font.data.size(), mode, nullptr, nullptr),
      hb_blob_destroy);
  std::unique_ptr<hb_face_t, decltype(&hb_face_destroy)> face(
      hb_face_create(blob.get(), 0), hb_face_destroy);
  HbFont result(hb_font_create(face.get()), hb_font_destroy);
  hb_ot_font_set_funcs(result.get());
  hb_font_set_scale(result.get(), font.unitsPerEm, font.unitsPerEm);
  hb_font_make_immutable(result.get());
  return result;
}

HbFont GetFont(const TtfFontMetrics &font) {
  if (font.resourceId.empty()) return MakeFont(font, HB_MEMORY_MODE_READONLY);
  static std::mutex mutex;
  static std::map<std::string, HbFont, std::less<>> cache;
  const std::lock_guard lock(mutex);
  if (auto it = cache.find(font.resourceId); it != cache.end()) return it->second;
  if (cache.size() >= 64) cache.clear();
  return cache.emplace(std::string(font.resourceId), MakeFont(font, HB_MEMORY_MODE_DUPLICATE)).first->second;
}

std::vector<int32_t> Breaks(const UnicodeLine &line, UBreakIteratorType type) {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<UBreakIterator, decltype(&ubrk_close)> iterator(
      ubrk_open(type, "root", line.utf16.data(), line.utf16.size(), &error), ubrk_close);
  if (U_FAILURE(error)) return {0, static_cast<int32_t>(line.utf16.size())};
  std::vector<int32_t> offsets;
  for (auto i = ubrk_first(iterator.get()); i != UBRK_DONE; i = ubrk_next(iterator.get()))
    offsets.push_back(i);
  return offsets;
}

struct Span {
  int32_t first, end;
  hb_script_t script;
  const TtfFontMetrics *font;
  const ShapingStyle *style;
};

bool IsStrong(hb_script_t script) {
  return script != HB_SCRIPT_COMMON && script != HB_SCRIPT_INHERITED && script != HB_SCRIPT_UNKNOWN;
}

void ShapeSpan(ShapedLine &line, const UnicodeLine &source, std::string_view original,
               const Span &span, bool rtl) {
  const auto font = GetFont(*span.font);
  std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer(
      hb_buffer_create(), hb_buffer_destroy);
  hb_buffer_add_utf16(buffer.get(), reinterpret_cast<const uint16_t *>(source.utf16.data()),
                      source.utf16.size(), span.first, span.end - span.first);
  hb_buffer_set_direction(buffer.get(), rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  hb_buffer_set_script(buffer.get(), span.script);
  hb_buffer_set_language(buffer.get(), hb_language_from_string("und", -1));
  hb_shape(font.get(), buffer.get(), nullptr, 0);
  unsigned count;
  const auto *infos = hb_buffer_get_glyph_infos(buffer.get(), &count);
  const auto *positions = hb_buffer_get_glyph_positions(buffer.get(), nullptr);
  const auto scale = span.style->size / span.font->unitsPerEm;
  // HarfBuzz clusters are monotone in shaping direction. Collect their source
  // ends in logical order so ligatures and reordered marks retain their text.
  std::vector<unsigned> ends;
  for (unsigned i = 0; i < count; ++i) ends.push_back(infos[i].cluster);
  ends.push_back(span.end);
  std::sort(ends.begin(), ends.end());
  ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
  for (unsigned i = 0; i < count; ++i) {
    const auto cluster = infos[i].cluster;
    const auto start = source.bytes[cluster];
    const auto end = source.bytes[*std::upper_bound(ends.begin(), ends.end(), cluster)];
    const auto advance = positions[i].x_advance * scale;
    const auto dx = positions[i].x_offset * scale, dy = positions[i].y_offset * scale;
    const bool continuation = i > 0 && infos[i - 1].cluster == cluster;
    const auto glyphId = static_cast<uint16_t>(infos[i].codepoint);
    UChar32 scalar;
    auto offset = static_cast<int32_t>(start);
    U8_NEXT(reinterpret_cast<const uint8_t *>(original.data()), offset, original.size(), scalar);
    if (!glyphId) line.unsupportedBytes.push_back(start);
    line.glyphs.push_back({std::string(original.substr(start, end - start)), line.width + dx,
        dy, advance, span.font->resourceId, static_cast<uint32_t>(scalar < 0 ? 0xfffd : scalar),
        glyphId, start, end, dx, dy, static_cast<uint32_t>(span.script), rtl, continuation, span.style->index});
    hb_glyph_extents_t extents{};
    if (hb_font_get_glyph_extents(font.get(), glyphId, &extents)) {
      auto &ink = line.glyphs.back().ink;
      ink.left = extents.x_bearing * scale;
      ink.top = extents.y_bearing * scale;
      ink.right = ink.left + extents.width * scale;
      ink.bottom = ink.top + extents.height * scale;
    }
    line.width += advance;
  }
}
}

bool IsRightToLeftScalar(uint32_t codepoint) {
  const auto direction = u_charDirection(codepoint);
  return direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC;
}

std::u16string DecodeUtf16(std::string_view text) {
  if (text.size() > INT32_MAX) return {};
  return UnicodeLine(text, false).utf16;
}

std::vector<size_t> UnicodeLineBreaks(std::string_view text) {
  const UnicodeLine source(text);
  std::vector<size_t> result;
  for (const auto boundary : Breaks(source, UBRK_LINE))
    result.push_back(source.bytes[boundary]);
  return result;
}

ShapedLine ShapeUnicodeLine(std::string_view text, double size,
                            std::span<const TtfFontMetrics *const> fonts,
                            std::string_view paragraph, size_t paragraphOffset) {
  const ShapingStyle style{0, 0, size, {fonts.begin(), fonts.end()}};
  return ShapeStyledLine(text, std::span(&style, 1), paragraph, paragraphOffset);
}

ShapedLine ShapeStyledLine(std::string_view text, std::span<const ShapingStyle> styles,
                           std::string_view paragraph, size_t paragraphOffset) {
  ShapedLine line;
  if (text.empty() || styles.empty() || text.size() > INT32_MAX) return line;
  const UnicodeLine source(text);
  line.unsupportedBytes = source.invalid;
  const auto boundaries = Breaks(source, UBRK_CHARACTER);
  std::vector<Span> clusters;
  auto style = styles.begin();
  for (size_t i = 1; i < boundaries.size(); ++i) {
    const auto first = boundaries[i - 1], end = boundaries[i];
    while (std::next(style) != styles.end() && std::next(style)->start <= source.bytes[first]) ++style;
    const auto &fonts = style->fonts;
    if (fonts.empty()) { line.unsupportedBytes.push_back(source.bytes[first]); continue; }
    const auto supports = [&](const auto *font) {
      for (auto pos = first; pos < end;) {
        UChar32 cp;
        U16_NEXT(source.utf16.data(), pos, end, cp);
        if (!u_hasBinaryProperty(cp, UCHAR_DEFAULT_IGNORABLE_CODE_POINT) &&
            !font->GlyphFor(cp)) return false;
      }
      return true;
    };
    const auto found = std::find_if(fonts.begin(), fonts.end(), supports);
    if (found == fonts.end()) line.unsupportedBytes.push_back(source.bytes[first]);
    auto script = HB_SCRIPT_COMMON;
    for (auto pos = first; pos < end && !IsStrong(script);) {
      UChar32 cp;
      U16_NEXT(source.utf16.data(), pos, end, cp);
      script = hb_unicode_script(hb_unicode_funcs_get_default(), cp);
    }
    clusters.push_back({first, end, script, found != fonts.end() ? *found : fonts.front(), &*style});
  }
  // Wrapped lines inherit the paragraph's embedding levels and base direction.
  // ICU applies line-specific whitespace rules before the line is shaped again.
  const bool useParagraph = !paragraph.empty();
  const UnicodeLine parent(paragraph);
  const auto &bidiText = useParagraph ? parent : source;
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<UBiDi, decltype(&ubidi_close)> bidiParent(ubidi_open(), ubidi_close);
  std::unique_ptr<UBiDi, decltype(&ubidi_close)> bidiLine(nullptr, ubidi_close);
  if (!bidiParent) { line.unsupportedBytes.push_back(0); return line; }
  ubidi_setPara(bidiParent.get(), bidiText.utf16.data(), bidiText.utf16.size(), UBIDI_DEFAULT_LTR, nullptr, &error);
  auto *bidi = bidiParent.get();
  if (useParagraph && U_SUCCESS(error)) {
    const auto start = std::lower_bound(parent.bytes.begin(), parent.bytes.end(), paragraphOffset) - parent.bytes.begin();
    bidiLine.reset(ubidi_open());
    if (!bidiLine) { line.unsupportedBytes.push_back(0); return line; }
    ubidi_setLine(bidiParent.get(), start, start + source.utf16.size(), bidiLine.get(), &error);
    bidi = bidiLine.get();
  }
  const int32_t runs = ubidi_countRuns(bidi, &error);
  if (U_FAILURE(error)) { line.unsupportedBytes.push_back(0); return line; }
  for (int32_t i = 0; i < runs; ++i) {
    int32_t start, length;
    const bool rtl = ubidi_getVisualRun(bidi, i, &start, &length) == UBIDI_RTL;
    const auto begin = std::lower_bound(clusters.begin(), clusters.end(), start,
        [](const Span &cluster, int32_t pos) { return cluster.first < pos; });
    const auto end = std::lower_bound(begin, clusters.end(), start + length,
        [](const Span &cluster, int32_t pos) { return cluster.first < pos; });
    auto script = HB_SCRIPT_COMMON;
    for (auto it = begin; it != end && !IsStrong(script); ++it) script = it->script;
    std::vector<Span> spans;
    for (auto it = begin; it != end; ++it) {
      if (IsStrong(it->script)) script = it->script;
      if (!spans.empty() && spans.back().font == it->font && spans.back().script == script && spans.back().style == it->style)
        spans.back().end = it->end;
      else
        spans.push_back({it->first, it->end, script, it->font, it->style});
    }
    if (rtl) std::reverse(spans.begin(), spans.end());
    for (const auto &span : spans) ShapeSpan(line, source, text, span, rtl);
  }
  std::sort(line.unsupportedBytes.begin(), line.unsupportedBytes.end());
  line.unsupportedBytes.erase(std::unique(line.unsupportedBytes.begin(), line.unsupportedBytes.end()),
                              line.unsupportedBytes.end());
  return line;
}
}
