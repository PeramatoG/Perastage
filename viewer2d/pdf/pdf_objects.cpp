#include "pdf_objects.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <zlib.h>

namespace layout_pdf_internal {

FloatFormatter::FloatFormatter(int precision)
    : precision_(std::clamp(precision, 0, 6)) {}

std::string FloatFormatter::Format(double value) const {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(precision_) << value;
  return ss.str();
}

bool PdfDeflater::Compress(const std::string &input, std::string &output,
                           std::string &error) {
  if (input.empty()) {
    output.clear();
    return true;
  }
  uLongf bound = compressBound(input.size());
  std::string compressed;
  compressed.resize(bound);

  int zres = compress2(reinterpret_cast<Bytef *>(compressed.data()), &bound,
                       reinterpret_cast<const Bytef *>(input.data()),
                       input.size(), Z_BEST_SPEED);
  if (zres != Z_OK) {
    error = "compress2 failed";
    return false;
  }

  compressed.resize(bound);
  output.swap(compressed);
  return true;
}

bool AppendEmbeddedFontObjects(std::vector<PdfObject> &objects,
                               PdfFontDefinition &font) {
  if (!font.metrics.valid || font.metrics.data.empty())
    return false;
  const double scale = font.metrics.unitsPerEm > 0 ? 1000.0 / font.metrics.unitsPerEm : 1.0;
  int ascent = static_cast<int>(std::lround(font.metrics.ascent * scale));
  int descent = -static_cast<int>(std::lround(std::abs(font.metrics.descent) * scale));
  int capHeight = static_cast<int>(std::lround(font.metrics.capHeight * scale));
  int xMin = static_cast<int>(std::lround(font.metrics.xMin * scale));
  int yMin = static_cast<int>(std::lround(font.metrics.yMin * scale));
  int xMax = static_cast<int>(std::lround(font.metrics.xMax * scale));
  int yMax = static_cast<int>(std::lround(font.metrics.yMax * scale));

  size_t fontFileIndex = objects.size() + 1;
  std::ostringstream fontFileStream;
  const bool needsNewline = font.metrics.data.empty() || font.metrics.data.back() != '\n';
  const size_t streamLength = font.metrics.data.size() + (needsNewline ? 1u : 0u);
  fontFileStream << "<< /Length " << streamLength << " /Length1 " << font.metrics.data.size()
                 << " >>\nstream\n" << font.metrics.data;
  if (needsNewline)
    fontFileStream << '\n';
  fontFileStream << "endstream";
  objects.push_back({fontFileStream.str()});

  size_t descriptorIndex = objects.size() + 1;
  std::ostringstream descriptor;
  descriptor << "<< /Type /FontDescriptor /FontName /" << font.baseName
             << " /Flags 32 /FontBBox [" << xMin << ' ' << yMin << ' ' << xMax << ' ' << yMax
             << "] /Ascent " << ascent << " /Descent " << descent << " /CapHeight " << capHeight
             << " /ItalicAngle 0 /StemV 80 /FontFile2 " << fontFileIndex << " 0 R >>";
  objects.push_back({descriptor.str()});

  size_t fontIndex = objects.size() + 1;
  std::ostringstream fontObject;
  fontObject << "<< /Type /Font /Subtype /TrueType /BaseFont /" << font.baseName
             << " /FirstChar 32 /LastChar 255 /Widths [";
  for (int code = 32; code <= 255; ++code) {
    fontObject << font.metrics.widths1000[static_cast<unsigned char>(code)];
    if (code != 255)
      fontObject << ' ';
  }
  fontObject << "] /FontDescriptor " << descriptorIndex << " 0 R /Encoding /WinAnsiEncoding >>";
  objects.push_back({fontObject.str()});

  font.objectId = fontIndex;
  font.embedded = true;
  return true;
}

void AppendFallbackType1Font(std::vector<PdfObject> &objects,
                             PdfFontDefinition &font,
                             const std::string &baseFont) {
  objects.push_back({"<< /Type /Font /Subtype /Type1 /BaseFont /" + baseFont + " >>"});
  font.objectId = objects.size();
  font.embedded = false;
  font.baseName = baseFont;
}

} // namespace layout_pdf_internal
