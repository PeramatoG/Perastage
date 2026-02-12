#pragma once

#include <array>
#include <filesystem>
#include <string>

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

std::string EncodeWinAnsi(const std::string &utf8);
double MeasureTextWidth(const std::string &text, double fontSize,
                        const PdfFontDefinition *font);
bool LoadPdfFontMetrics(PdfFontDefinition &font, bool bold);
