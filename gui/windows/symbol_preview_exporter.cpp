#include "windows/symbol_preview_exporter.h"

#include <fstream>
#include <sstream>

namespace symbol_preview {
namespace {

std::string BuildPoints(const symbols::Polyline2D &line,
                        const symbols::Aabb2D &bounds) {
  std::ostringstream stream;
  bool first = true;
  for (const auto &point : line) {
    if (!first)
      stream << ' ';
    first = false;
    const float x = point.x - bounds.min.x;
    const float y = bounds.max.y - point.y;
    stream << x << ',' << y;
  }
  return stream.str();
}

bool BuildSvgContent(const symbols::Symbol2D &symbol, std::string &svgContent,
                     std::string &errorMessage) {
  if (!symbol.bounds.valid) {
    errorMessage = "The selected view has no drawable symbol.";
    return false;
  }

  const float width = symbol.bounds.max.x - symbol.bounds.min.x;
  const float height = symbol.bounds.max.y - symbol.bounds.min.y;
  if (width <= 0.0f || height <= 0.0f) {
    errorMessage = "The selected view has invalid bounds.";
    return false;
  }

  std::ostringstream file;
  file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  file << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
          "viewBox=\"0 0 "
       << width << ' ' << height << "\">\n";

  for (const auto &polygon : symbol.fill) {
    if (polygon.outer.size() < 3)
      continue;

    file << "  <polygon points=\"" << BuildPoints(polygon.outer, symbol.bounds)
         << "\" fill=\"#e0e0e0\" stroke=\"none\"/>\n";
    for (const auto &hole : polygon.holes) {
      if (hole.size() < 3)
        continue;
      file << "  <polygon points=\"" << BuildPoints(hole, symbol.bounds)
           << "\" fill=\"#ffffff\" stroke=\"none\"/>\n";
    }
  }

  const float strokeWidth = symbol.strokeWidthPx < 0.0f ? 0.0f : symbol.strokeWidthPx;
  for (const auto &line : symbol.strokes) {
    if (line.size() < 2)
      continue;
    file << "  <polyline points=\"" << BuildPoints(line, symbol.bounds)
         << "\" fill=\"none\" stroke=\"#000000\" stroke-width=\""
         << strokeWidth << "\"/>\n";
  }

  file << "</svg>\n";
  if (!file.good()) {
    errorMessage = "Could not write the SVG data to disk.";
    return false;
  }

  svgContent = file.str();
  return true;
}

} // namespace

bool ExportSymbolToSvg(const symbols::Symbol2D &symbol,
                       const std::string &filePath,
                       std::string &errorMessage) {
  std::string svgContent;
  if (!BuildSvgContent(symbol, svgContent, errorMessage))
    return false;

  std::ofstream file(filePath);
  if (!file.is_open()) {
    errorMessage = "Could not open the output file.";
    return false;
  }

  file << svgContent;
  return file.good();
}

bool ExportSymbolToSvgString(const symbols::Symbol2D &symbol,
                             std::string &svgContent,
                             std::string &errorMessage) {
  return BuildSvgContent(symbol, svgContent, errorMessage);
}

} // namespace symbol_preview
