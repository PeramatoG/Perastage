#include "standard_gdtf_svg.h"

#include "Symbol2DSvg.h"

#include <cmath>
#include <locale>
#include <sstream>

#include <tinyxml2.h>

namespace symbols {

std::string StandardGdtfSvgPath(const std::string &modelFile,
                                SymbolViewKind view) {
  const char *directory = nullptr;
  switch (view) {
  case SymbolViewKind::Top:
    directory = "models/svg/";
    break;
  case SymbolViewKind::Front:
    directory = "models/svg_front/";
    break;
  case SymbolViewKind::Left:
  case SymbolViewKind::Right:
    directory = "models/svg_side/";
    break;
  default:
    return {};
  }
  return std::string(directory) + modelFile + ".svg";
}

bool SerializeStandardGdtfSvg(const Symbol2D &symbol, SymbolViewKind view,
                             StandardGdtfSvgCandidate &candidate,
                             std::string &errorMessage) {
  candidate = {};
  if (StandardGdtfSvgPath("model", view).empty()) {
    errorMessage = "GDTF 1.2 supports only Top, Side and Front SVG views.";
    return false;
  }
  const auto finite = [](const Point2D &point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
  };
  if (!finite(symbol.bounds.min) || !finite(symbol.bounds.max) ||
      !std::isfinite(symbol.strokeWidthPx)) {
    errorMessage = "Standard GDTF SVG geometry must be finite.";
    return false;
  }
  for (const auto &polygon : symbol.fill) {
    for (const auto &point : polygon.outer)
      if (!finite(point)) {
        errorMessage = "Standard GDTF SVG geometry must be finite.";
        return false;
      }
    for (const auto &hole : polygon.holes)
      for (const auto &point : hole)
        if (!finite(point)) {
          errorMessage = "Standard GDTF SVG geometry must be finite.";
          return false;
        }
  }
  for (const auto &line : symbol.strokes)
    for (const auto &point : line)
      if (!finite(point)) {
        errorMessage = "Standard GDTF SVG geometry must be finite.";
        return false;
      }
  if (!SerializeSymbolToSvg(symbol, candidate.svg, errorMessage))
    return false;
  tinyxml2::XMLDocument document;
  if (document.Parse(candidate.svg.c_str(), candidate.svg.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not serialize the standard GDTF SVG.";
    return false;
  }
  auto dimension = [](double value) {
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << value << "mm";
    return text.str();
  };
  auto *svg = document.FirstChildElement("svg");
  svg->SetAttribute("width", dimension(symbol.bounds.max.x - symbol.bounds.min.x).c_str());
  svg->SetAttribute("height", dimension(symbol.bounds.max.y - symbol.bounds.min.y).c_str());
  tinyxml2::XMLPrinter printer;
  document.Print(&printer);
  candidate.svg = printer.CStr();
  candidate.viewKind = view == SymbolViewKind::Right ? SymbolViewKind::Left : view;
  candidate.offsetXmm = -symbol.bounds.min.x;
  candidate.offsetYmm = -symbol.bounds.min.y;
  errorMessage.clear();
  return true;
}

} // namespace symbols
