#include "symbols/Symbol2DSvg.h"

#include <algorithm>
#include <locale>
#include <sstream>

namespace symbols {
namespace {

std::string BuildPoints(const Polyline2D &line, const Aabb2D &bounds) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
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

} // namespace

bool SerializeSymbolToSvg(const Symbol2D &symbol, std::string &svgContent,
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
  file.imbue(std::locale::classic());
  file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  file << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
          "viewBox=\"0 0 "
       << width << ' ' << height << "\">\n";

  for (const auto &polygon : symbol.fill) {
    if (polygon.outer.size() < 3)
      continue;

    const bool hasHoles = std::any_of(
        polygon.holes.begin(), polygon.holes.end(),
        [](const Polyline2D &hole) { return hole.size() >= 3; });
    if (!hasHoles) {
      file << "  <polygon points=\"" << BuildPoints(polygon.outer, symbol.bounds)
           << "\" fill=\"#e0e0e0\" stroke=\"none\"/>\n";
      continue;
    }

    // Each filled region is one shape; even-odd subpaths expose the background.
    file << "  <path d=\"";
    auto writeRing = [&](const Polyline2D &ring) {
      for (size_t i = 0; i < ring.size(); ++i) {
        file << (i == 0 ? "M " : " L ")
             << ring[i].x - symbol.bounds.min.x << ','
             << symbol.bounds.max.y - ring[i].y;
      }
      file << " Z";
    };
    writeRing(polygon.outer);
    for (const auto &hole : polygon.holes) {
      if (hole.size() < 3)
        continue;
      file << ' ';
      writeRing(hole);
    }
    file << "\" fill=\"#e0e0e0\" fill-rule=\"evenodd\" stroke=\"none\"/>\n";
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

} // namespace symbols
