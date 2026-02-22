#include "windows/symbol_preview_exporter.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace symbol_preview {
namespace {

symbols::Point2D NormalizePointToBounds(const symbols::Point2D &point,
                                        const symbols::Aabb2D &bounds) {
  return symbols::Point2D{point.x - bounds.min.x, bounds.max.y - point.y};
}

void ExtendBounds(symbols::Aabb2D &bounds, const symbols::Point2D &point) {
  if (!bounds.valid) {
    bounds.min = point;
    bounds.max = point;
    bounds.valid = true;
    return;
  }
  bounds.min.x = std::min(bounds.min.x, point.x);
  bounds.min.y = std::min(bounds.min.y, point.y);
  bounds.max.x = std::max(bounds.max.x, point.x);
  bounds.max.y = std::max(bounds.max.y, point.y);
}

symbols::Aabb2D BuildNormalizedPhysicalBounds(const symbols::Symbol2D &symbol) {
  symbols::Aabb2D normalized;
  if (!symbol.bounds.valid)
    return normalized;

  for (const auto &polygon : symbol.fill) {
    for (const auto &point : polygon.outer)
      ExtendBounds(normalized, NormalizePointToBounds(point, symbol.bounds));
    for (const auto &hole : polygon.holes)
      for (const auto &point : hole)
        ExtendBounds(normalized, NormalizePointToBounds(point, symbol.bounds));
  }

  for (const auto &line : symbol.strokes)
    for (const auto &point : line)
      ExtendBounds(normalized, NormalizePointToBounds(point, symbol.bounds));

  if (!normalized.valid)
    normalized = symbols::Aabb2D{symbols::Point2D{0.0f, 0.0f},
                                 symbols::Point2D{symbol.bounds.max.x - symbol.bounds.min.x,
                                                  symbol.bounds.max.y - symbol.bounds.min.y},
                                 true};
  return normalized;
}


std::string BuildPoints(const symbols::Polyline2D &line,
                        const symbols::Aabb2D &sourceBounds) {
  std::ostringstream stream;
  bool first = true;
  for (const auto &point : line) {
    if (!first)
      stream << ' ';
    first = false;
    const symbols::Point2D normalizedPoint =
        NormalizePointToBounds(point, sourceBounds);
    const float x = normalizedPoint.x;
    const float y = normalizedPoint.y;
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

  const symbols::Aabb2D normalizedBounds = BuildNormalizedPhysicalBounds(symbol);
  const float width = normalizedBounds.valid ? (normalizedBounds.max.x - normalizedBounds.min.x) : 0.0f;
  const float height = normalizedBounds.valid ? (normalizedBounds.max.y - normalizedBounds.min.y) : 0.0f;
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
