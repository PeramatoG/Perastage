#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace typography {
enum class FontIssue {
  None, Missing, Unapproved, UnsupportedFormat, FaceUnavailable,
  Invalid, EmbeddingUnverified, Inaccessible
};

struct TtfFontMetrics {
  int unitsPerEm = 1000;
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  int capHeight = 0;
  double italicAngle = 0;
  int xMin = 0;
  int yMin = 0;
  int xMax = 0;
  int yMax = 0;
  std::string data;
  std::string_view resourceId;
  std::vector<int> glyphAdvances;
  uint32_t cmap4 = 0, cmap12 = 0;
  uint16_t GlyphFor(uint32_t codepoint) const;
  bool valid = false;
};

bool ReadFileToString(const std::filesystem::path &path, std::string &out);
bool FindTable(const std::string &data, uint32_t tag, uint32_t &offset,
               uint32_t &length);
FontIssue CheckStaticTrueType(const std::string &data);
bool ParseTtfFontMetrics(std::string data, TtfFontMetrics &metrics);
bool LoadTtfFontMetrics(const std::filesystem::path &path,
                        TtfFontMetrics &metrics);


} // namespace typography
