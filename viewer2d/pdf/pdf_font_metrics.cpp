#include "pdf_font_metrics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace layout_pdf_internal {

std::string ToLowerCopy(std::string_view input) {
  std::string lower;
  lower.reserve(input.size());
  for (char ch : input)
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  return lower;
}

const PdfFontDefinition *PdfFontCatalog::Resolve(
    const std::string &family) const {
  const PdfFontDefinition *fallback = regular ? regular : bold;
  if (!fallback)
    return nullptr;
  if (family.empty())
    return fallback;
  std::string lower = ToLowerCopy(family);
  if (bold && lower.find("bold") != std::string::npos)
    return bold;
  if (lower.find("sans") != std::string::npos ||
      lower.find("arial") != std::string::npos ||
      lower.find("dejavu") != std::string::npos)
    return regular ? regular : bold;
  return fallback;
}

double MeasureTextWidth(const std::string &text, double fontSize,
                        const PdfFontDefinition *font) {
  if (!font || !font->embedded || font->metrics.unitsPerEm <= 0)
    return static_cast<double>(text.size()) * fontSize * 0.6;
  double units = 0.0;
  for (unsigned char ch : text) {
    if (ch == '\n')
      continue;
    units += font->metrics.advanceWidths[ch];
  }
  return (units / font->metrics.unitsPerEm) * fontSize;
}

unsigned char EncodeWinAnsiCodepoint(uint32_t codepoint) {
  if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t')
    return static_cast<unsigned char>(codepoint);
  if (codepoint <= 0x7F)
    return static_cast<unsigned char>(codepoint);
  if (codepoint >= 0xA0 && codepoint <= 0xFF)
    return static_cast<unsigned char>(codepoint);
  switch (codepoint) {
  case 0x20AC:
    return 0x80;
  case 0x201A:
    return 0x82;
  case 0x0192:
    return 0x83;
  case 0x201E:
    return 0x84;
  case 0x2026:
    return 0x85;
  case 0x2020:
    return 0x86;
  case 0x2021:
    return 0x87;
  case 0x02C6:
    return 0x88;
  case 0x2030:
    return 0x89;
  case 0x0160:
    return 0x8A;
  case 0x2039:
    return 0x8B;
  case 0x0152:
    return 0x8C;
  case 0x017D:
    return 0x8E;
  case 0x2018:
    return 0x91;
  case 0x2019:
    return 0x92;
  case 0x201C:
    return 0x93;
  case 0x201D:
    return 0x94;
  case 0x2022:
    return 0x95;
  case 0x2013:
    return 0x96;
  case 0x2014:
    return 0x97;
  case 0x02DC:
    return 0x98;
  case 0x2122:
    return 0x99;
  case 0x0161:
    return 0x9A;
  case 0x203A:
    return 0x9B;
  case 0x0153:
    return 0x9C;
  case 0x017E:
    return 0x9E;
  case 0x0178:
    return 0x9F;
  default:
    return '?';
  }
}

std::string EncodeWinAnsi(const std::string &utf8) {
  std::string out;
  out.reserve(utf8.size());
  size_t i = 0;
  while (i < utf8.size()) {
    unsigned char lead = static_cast<unsigned char>(utf8[i]);
    uint32_t codepoint = 0;
    size_t length = 0;
    if (lead < 0x80) {
      codepoint = lead;
      length = 1;
    } else if ((lead >> 5) == 0x6 && i + 1 < utf8.size()) {
      codepoint = ((lead & 0x1F) << 6) |
                  (static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
      length = 2;
    } else if ((lead >> 4) == 0xE && i + 2 < utf8.size()) {
      codepoint = ((lead & 0x0F) << 12) |
                  ((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6) |
                  (static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
      length = 3;
    } else if ((lead >> 3) == 0x1E && i + 3 < utf8.size()) {
      codepoint = ((lead & 0x07) << 18) |
                  ((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12) |
                  ((static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6) |
                  (static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
      length = 4;
    } else {
      out.push_back('?');
      ++i;
      continue;
    }
    out.push_back(static_cast<char>(EncodeWinAnsiCodepoint(codepoint)));
    i += length;
  }
  return out;
}

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
      return entryOffset + entryLength <= data.size();
    }
  }
  return false;
}

bool LoadTtfFontMetrics(const std::filesystem::path &path,
                        TtfFontMetrics &metrics) {
  metrics = TtfFontMetrics{};
  std::string data;
  if (!ReadFileToString(path, data))
    return false;
  if (data.size() < 12)
    return false;

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

  if (headOffset + 54 > data.size())
    return false;
  metrics.unitsPerEm = ReadU16(data, headOffset + 18);
  metrics.xMin = ReadS16(data, headOffset + 36);
  metrics.yMin = ReadS16(data, headOffset + 38);
  metrics.xMax = ReadS16(data, headOffset + 40);
  metrics.yMax = ReadS16(data, headOffset + 42);

  if (hheaOffset + 36 > data.size())
    return false;
  metrics.ascent = ReadS16(data, hheaOffset + 4);
  metrics.descent = ReadS16(data, hheaOffset + 6);
  metrics.lineGap = ReadS16(data, hheaOffset + 8);
  uint16_t numHMetrics = ReadU16(data, hheaOffset + 34);

  if (maxpOffset + 6 > data.size())
    return false;
  uint16_t numGlyphs = ReadU16(data, maxpOffset + 4);
  if (numGlyphs == 0)
    return false;

  if (numHMetrics == 0)
    return false;

  if (hmtxOffset + static_cast<uint32_t>(numHMetrics) * 4 > data.size())
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
  if (metrics.capHeight == 0)
    metrics.capHeight = metrics.ascent;

  const std::string cmapData =
      data.substr(cmapOffset, std::min<size_t>(cmapLength, data.size() - cmapOffset));
  if (cmapData.size() < 4)
    return false;
  uint16_t cmapTables = ReadU16(cmapData, 2);
  size_t cmapRecordOffset = 4;
  uint32_t chosenOffset = 0;
  for (uint16_t i = 0; i < cmapTables; ++i) {
    if (cmapRecordOffset + 8 > cmapData.size())
      return false;
    uint16_t platformId = ReadU16(cmapData, cmapRecordOffset);
    uint16_t encodingId = ReadU16(cmapData, cmapRecordOffset + 2);
    uint32_t subOffset = ReadU32(cmapData, cmapRecordOffset + 4);
    size_t subBase = subOffset;
    if (subBase + 2 > cmapData.size())
      continue;
    uint16_t format = ReadU16(cmapData, subBase);
    if (format == 4 && platformId == 3 &&
        (encodingId == 1 || encodingId == 0)) {
      chosenOffset = subOffset;
      break;
    }
    cmapRecordOffset += 8;
  }
  if (chosenOffset == 0)
    return false;

  size_t subBase = chosenOffset;
  if (subBase + 14 > cmapData.size())
    return false;
  uint16_t segCount = ReadU16(cmapData, subBase + 6) / 2;
  size_t endCountOffset = subBase + 14;
  size_t startCountOffset = endCountOffset + 2 * segCount + 2;
  size_t idDeltaOffset = startCountOffset + 2 * segCount;
  size_t idRangeOffsetOffset = idDeltaOffset + 2 * segCount;
  if (idRangeOffsetOffset + 2 * segCount > cmapData.size())
    return false;

  auto glyphForCodepoint = [&](uint16_t code) -> uint16_t {
    for (uint16_t i = 0; i < segCount; ++i) {
      uint16_t endCount = ReadU16(cmapData, endCountOffset + 2 * i);
      uint16_t startCount = ReadU16(cmapData, startCountOffset + 2 * i);
      if (code < startCount || code > endCount)
        continue;
      int16_t idDelta = ReadS16(cmapData, idDeltaOffset + 2 * i);
      uint16_t idRangeOffset = ReadU16(cmapData, idRangeOffsetOffset + 2 * i);
      if (idRangeOffset == 0) {
        return static_cast<uint16_t>(code + idDelta);
      }
      size_t glyphOffset =
          idRangeOffsetOffset + 2 * i + idRangeOffset + 2 * (code - startCount);
      if (glyphOffset + 2 > cmapData.size())
        return 0;
      uint16_t glyphIndex = ReadU16(cmapData, glyphOffset);
      if (glyphIndex == 0)
        return 0;
      return static_cast<uint16_t>(glyphIndex + idDelta);
    }
    return 0;
  };

  int missingWidth = advanceWidths.empty() ? 0 : advanceWidths[0];
  for (size_t i = 0; i < metrics.advanceWidths.size(); ++i) {
    uint16_t glyphIndex = glyphForCodepoint(static_cast<uint16_t>(i));
    int advance = missingWidth;
    if (glyphIndex < advanceWidths.size())
      advance = advanceWidths[glyphIndex];
    metrics.advanceWidths[i] = advance;
    if (metrics.unitsPerEm > 0) {
      metrics.widths1000[i] =
          static_cast<int>(std::lround(advance * 1000.0 / metrics.unitsPerEm));
    } else {
      metrics.widths1000[i] = 0;
    }
  }

  metrics.data = std::move(data);
  metrics.valid = metrics.unitsPerEm > 0;
  return metrics.valid;
}

std::filesystem::path FindFontPath(bool bold) {
  struct FontCandidate {
    const char *regularPath;
    const char *boldPath;
  };
  // Keep this list in sync with the font face names used by the UI legend/event
  // table rendering so PDFs and on-screen views share the same family.
  const std::vector<FontCandidate> candidates = {
#ifdef _WIN32
      {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/arialbd.ttf"},
#elif defined(__APPLE__)
      {"/Library/Fonts/Arial.ttf", "/Library/Fonts/Arial Bold.ttf"},
      {"/System/Library/Fonts/Supplemental/Arial.ttf",
       "/System/Library/Fonts/Supplemental/Arial Bold.ttf"},
#else
      {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
       "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
      {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
       "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
#endif
  };
  for (const auto &candidate : candidates) {
    const char *path = bold ? candidate.boldPath : candidate.regularPath;
    if (path && std::filesystem::exists(path))
      return std::filesystem::path(path);
  }
  return {};
}

bool LoadPdfFontMetrics(PdfFontDefinition &font, bool bold) {
  std::filesystem::path path = FindFontPath(bold);
  if (path.empty())
    return false;
  return LoadTtfFontMetrics(path, font.metrics);
}


} // namespace layout_pdf_internal
