#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace layout_pdf_internal {

struct TtfFontMetrics {
  int unitsPerEm = 1000;
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  int capHeight = 0;
  int xMin = 0;
  int yMin = 0;
  int xMax = 0;
  int yMax = 0;
  std::array<int, 256> advanceWidths{};
  std::array<int, 256> widths1000{};
  std::string data;
  bool valid = false;
};

struct PdfFontDefinition {
  std::string key;
  std::string family;
  std::string baseName;
  size_t objectId = 0;
  bool embedded = false;
  TtfFontMetrics metrics;
};

struct PdfFontCatalog {
  const PdfFontDefinition *regular = nullptr;
  const PdfFontDefinition *bold = nullptr;

  const PdfFontDefinition *Resolve(const std::string &family) const;
};

std::string ToLowerCopy(std::string_view input);
double MeasureTextWidth(const std::string &text, double fontSize,
                        const PdfFontDefinition *font);
std::string EncodeWinAnsi(const std::string &utf8);

bool ReadFileToString(const std::filesystem::path &path, std::string &out);
bool FindTable(const std::string &data, uint32_t tag, uint32_t &offset,
               uint32_t &length);
bool LoadTtfFontMetrics(const std::filesystem::path &path,
                        TtfFontMetrics &metrics);
bool LoadPdfFontMetrics(PdfFontDefinition &font, bool bold);

} // namespace layout_pdf_internal
