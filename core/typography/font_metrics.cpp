#include "font_metrics.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace typography {

bool ReadFileToString(const std::filesystem::path &path, std::string &out) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

uint16_t ReadU16(const std::string &data, size_t offset) {
  return static_cast<uint16_t>(
      (static_cast<unsigned char>(data[offset]) << 8) |
      static_cast<unsigned char>(data[offset + 1]));
}

int16_t ReadS16(const std::string &data, size_t offset) {
  return static_cast<int16_t>(ReadU16(data, offset));
}

uint32_t ReadU32(const std::string &data, size_t offset) {
  return (static_cast<uint32_t>(static_cast<unsigned char>(data[offset])) << 24) |
         (static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 16) |
         (static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 8) |
         static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 3]));
}

uint32_t MakeTag(char a, char b, char c, char d) {
  return (static_cast<uint32_t>(a) << 24) |
         (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(c) << 8) |
         static_cast<uint32_t>(d);
}

bool FindTable(const std::string &data, uint32_t tag, uint32_t &offset,
               uint32_t &length) {
  if (data.size() < 12)
    return false;
  uint16_t numTables = ReadU16(data, 4);
  size_t tableDir = 12;
  for (uint16_t i = 0; i < numTables; ++i) {
    size_t recordOffset = tableDir + i * 16;
    if (recordOffset + 16 > data.size())
      return false;
    uint32_t entryTag = ReadU32(data, recordOffset);
    uint32_t entryOffset = ReadU32(data, recordOffset + 8);
    uint32_t entryLength = ReadU32(data, recordOffset + 12);
    if (entryTag == tag) {
      offset = entryOffset;
      length = entryLength;
      return entryOffset <= data.size() && entryLength <= data.size() - entryOffset;
    }
  }
  return false;
}

FontIssue CheckStaticTrueType(const std::string &data) {
  if (data.size() < 12) return FontIssue::Invalid;
  if (ReadU32(data, 0) != 0x00010000) {
    const auto tag = ReadU32(data, 0);
    return tag == MakeTag('O','T','T','O') || tag == MakeTag('t','t','c','f') ||
           tag == MakeTag('w','O','F','F') || tag == MakeTag('w','O','F','2')
        ? FontIssue::UnsupportedFormat : FontIssue::Invalid;
  }
  uint32_t offset, length;
  if (!FindTable(data, MakeTag('g','l','y','f'), offset, length) ||
      !FindTable(data, MakeTag('l','o','c','a'), offset, length) ||
       FindTable(data, MakeTag('f','v','a','r'), offset, length))
    return FontIssue::UnsupportedFormat;
  if (!FindTable(data, MakeTag('O','S','/','2'), offset, length) ||
      length < 10 || ReadU16(data, offset + 8) != 0)
    return FontIssue::EmbeddingUnverified;
  return FontIssue::None;
}

bool LoadTtfFontMetrics(const std::filesystem::path &path, TtfFontMetrics &metrics) {
  std::string data;
  return ReadFileToString(path, data) && ParseTtfFontMetrics(std::move(data), metrics);
}

bool ParseTtfFontMetrics(std::string data, TtfFontMetrics &metrics) {
  metrics = TtfFontMetrics{};
  if (data.size() < 12) return false;

  uint32_t headOffset = 0, headLength = 0;
  uint32_t hheaOffset = 0, hheaLength = 0;
  uint32_t maxpOffset = 0, maxpLength = 0;
  uint32_t hmtxOffset = 0, hmtxLength = 0;
  uint32_t cmapOffset = 0, cmapLength = 0;
  uint32_t os2Offset = 0, os2Length = 0;

  if (!FindTable(data, MakeTag('h', 'e', 'a', 'd'), headOffset, headLength))
    return false;
  if (!FindTable(data, MakeTag('h', 'h', 'e', 'a'), hheaOffset, hheaLength))
    return false;
  if (!FindTable(data, MakeTag('m', 'a', 'x', 'p'), maxpOffset, maxpLength))
    return false;
  if (!FindTable(data, MakeTag('h', 'm', 't', 'x'), hmtxOffset, hmtxLength))
    return false;
  if (!FindTable(data, MakeTag('c', 'm', 'a', 'p'), cmapOffset, cmapLength))
    return false;
  FindTable(data, MakeTag('O', 'S', '/', '2'), os2Offset, os2Length);

  if (headLength < 54)
    return false;
  metrics.unitsPerEm = ReadU16(data, headOffset + 18);
  metrics.xMin = ReadS16(data, headOffset + 36);
  metrics.yMin = ReadS16(data, headOffset + 38);
  metrics.xMax = ReadS16(data, headOffset + 40);
  metrics.yMax = ReadS16(data, headOffset + 42);

  if (hheaLength < 36)
    return false;
  metrics.ascent = ReadS16(data, hheaOffset + 4);
  metrics.descent = ReadS16(data, hheaOffset + 6);
  metrics.lineGap = ReadS16(data, hheaOffset + 8);
  uint16_t numHMetrics = ReadU16(data, hheaOffset + 34);

  if (maxpLength < 6)
    return false;
  uint16_t numGlyphs = ReadU16(data, maxpOffset + 4);
  if (numGlyphs == 0)
    return false;

  if (numHMetrics == 0 || numHMetrics > numGlyphs)
    return false;

  if (hmtxLength < static_cast<uint32_t>(numHMetrics) * 4)
    return false;

  std::vector<int> advanceWidths;
  advanceWidths.resize(numGlyphs, 0);
  int lastAdvance = 0;
  for (uint16_t i = 0; i < numHMetrics; ++i) {
    size_t entry = hmtxOffset + static_cast<size_t>(i) * 4;
    int advance = ReadU16(data, entry);
    advanceWidths[i] = advance;
    lastAdvance = advance;
  }
  for (uint16_t i = numHMetrics; i < numGlyphs; ++i) {
    advanceWidths[i] = lastAdvance;
  }

  if (os2Offset != 0 && os2Length >= 90 && os2Offset + 90 <= data.size()) {
    uint16_t version = ReadU16(data, os2Offset);
    if (version >= 2) {
      metrics.capHeight = ReadS16(data, os2Offset + 88);
    }
  }
  uint32_t postOffset = 0, postLength = 0;
  if (FindTable(data, MakeTag('p','o','s','t'), postOffset, postLength) && postLength >= 8)
    metrics.italicAngle = static_cast<int32_t>(ReadU32(data, postOffset + 4)) / 65536.0;

  if (metrics.capHeight == 0)
    metrics.capHeight = metrics.ascent;

  if (cmapLength < 4) return false;
  const auto count = ReadU16(data, cmapOffset + 2);
  if (4u + 8u * count > cmapLength) return false;
  for (unsigned i = 0; i < count; ++i) {
    const auto record = cmapOffset + 4 + 8 * i;
    const auto platform = ReadU16(data, record);
    const auto encoding = ReadU16(data, record + 2);
    if (platform != 0 && (platform != 3 || (encoding != 1 && encoding != 10)))
      continue;
    const auto relative = ReadU32(data, record + 4);
    if (relative > cmapLength - 4) continue;
    const auto base = cmapOffset + relative;
    const auto format = ReadU16(data, base);
    if (format == 4) {
      const auto length = ReadU16(data, base + 2);
      if (length >= 16 && length <= cmapLength - relative &&
          16u + 4u * ReadU16(data, base + 6) <= length)
        metrics.cmap4 = base;
    } else if (format == 12 && cmapLength - relative >= 16) {
      const auto length = ReadU32(data, base + 4);
      if (length >= 16 && length <= cmapLength - relative &&
          ReadU32(data, base + 12) <= (length - 16) / 12)
        metrics.cmap12 = base;
    }
  }
  if (!metrics.cmap4 && !metrics.cmap12) return false;
  metrics.glyphAdvances = std::move(advanceWidths);
  metrics.data = std::move(data);
  metrics.valid = metrics.unitsPerEm > 0;
  return metrics.valid;
}

uint16_t TtfFontMetrics::GlyphFor(uint32_t codepoint) const {
  if (cmap12) {
    auto first = 0u;
    auto last = ReadU32(data, cmap12 + 12);
    while (first < last) {
      const auto middle = first + (last - first) / 2;
      const auto entry = cmap12 + 16 + 12 * middle;
      const auto start = ReadU32(data, entry);
      if (codepoint < start) last = middle;
      else if (codepoint > ReadU32(data, entry + 4)) first = middle + 1;
      else {
        const uint64_t glyph = uint64_t(ReadU32(data, entry + 8)) + codepoint - start;
        return glyph < glyphAdvances.size() ? static_cast<uint16_t>(glyph) : 0;
      }
    }
  }
  if (!cmap4 || codepoint > 0xffff) return 0;
  const auto count = ReadU16(data, cmap4 + 6) / 2u;
  const auto ends = cmap4 + 14;
  unsigned first = 0, last = count;
  while (first < last) {
    const auto middle = first + (last - first) / 2;
    if (ReadU16(data, ends + 2 * middle) < codepoint) first = middle + 1;
    else last = middle;
  }
  if (first == count) return 0;
  const auto starts = ends + 2 * count + 2;
  const auto start = ReadU16(data, starts + 2 * first);
  if (codepoint < start) return 0;
  const auto delta = ReadS16(data, starts + 2 * (count + first));
  const auto range = starts + 2 * (2 * count + first);
  const auto offset = ReadU16(data, range);
  uint16_t glyph = static_cast<uint16_t>(codepoint + delta);
  if (offset) {
    const auto entry = range + offset + 2 * (codepoint - start);
    if (entry + 2 > cmap4 + ReadU16(data, cmap4 + 2)) return 0;
    glyph = ReadU16(data, entry);
    if (glyph) glyph = static_cast<uint16_t>(glyph + delta);
  }
  return glyph < glyphAdvances.size() ? glyph : 0;
}

} // namespace typography
