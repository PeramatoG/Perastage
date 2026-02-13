#pragma once

#include "pdf_font_metrics.h"

#include <sstream>
#include <string>
#include <vector>

namespace layout_pdf_internal {

struct PdfObject {
  std::string body;
};

class FloatFormatter {
public:
  explicit FloatFormatter(int precision);
  std::string Format(double value) const;

private:
  int precision_;
};

class PdfDeflater {
public:
  static bool Compress(const std::string &input, std::string &output,
                       std::string &error);
};

bool AppendEmbeddedFontObjects(std::vector<PdfObject> &objects,
                               PdfFontDefinition &font);
void AppendFallbackType1Font(std::vector<PdfObject> &objects,
                             PdfFontDefinition &font,
                             const std::string &baseFont);

} // namespace layout_pdf_internal
