#include "pdf_font_metrics.h"
#include <vector>
#include <algorithm>
#include <cctype>

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
  if (family == "perastage-sans") return labelRegular;
  if (family == "perastage-sans-bold") return labelBold;
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
