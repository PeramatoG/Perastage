#pragma once
#include "typography/font_metrics.h"
namespace layout_pdf_internal {
using typography::TtfFontMetrics;
using typography::EncodeWinAnsi;
using typography::LoadTtfFontMetrics;
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
  const PdfFontDefinition *labelRegular = nullptr;
  const PdfFontDefinition *labelBold = nullptr;

  const PdfFontDefinition *Resolve(const std::string &family) const;
};

std::string ToLowerCopy(std::string_view input);
double MeasureTextWidth(const std::string &text, double fontSize,
                        const PdfFontDefinition *font);

bool LoadPdfFontMetrics(PdfFontDefinition &font, bool bold);
}
