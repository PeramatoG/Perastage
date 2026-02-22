#include "symbols/Symbol2DImageBuilder.h"
#include "windows/symbol_preview_exporter.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

void FillRect(symbols::RenderedSymbolImage &render, int minX, int minY, int maxX,
              int maxY) {
  render.rgba.assign(static_cast<size_t>(render.width) * static_cast<size_t>(render.height) *
                         4,
                     255);
  for (int y = minY; y < maxY; ++y) {
    for (int x = minX; x < maxX; ++x) {
      const size_t idx =
          (static_cast<size_t>(y) * static_cast<size_t>(render.width) +
           static_cast<size_t>(x)) *
          4;
      render.rgba[idx + 0] = 200;
      render.rgba[idx + 1] = 200;
      render.rgba[idx + 2] = 200;
      render.rgba[idx + 3] = 255;
    }
  }
}

float SymbolWidth(const symbols::Symbol2D &symbol) {
  return symbol.bounds.max.x - symbol.bounds.min.x;
}

float SymbolHeight(const symbols::Symbol2D &symbol) {
  return symbol.bounds.max.y - symbol.bounds.min.y;
}

} // namespace

int main() {
  symbols::RenderedSymbolImage top;
  top.view = symbols::SymbolView::Top;
  top.width = 100;
  top.height = 100;
  top.worldUnitsPerPixel = 0.02f;
  FillRect(top, 20, 30, 70, 50);

  symbols::RenderedSymbolImage front;
  front.view = symbols::SymbolView::Front;
  front.width = 200;
  front.height = 200;
  front.worldUnitsPerPixel = 0.04f;
  FillRect(front, 40, 110, 65, 120);

  const auto symbolsBuilt =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImages({top, front});
  if (symbolsBuilt.size() != 2) {
    std::cerr << "Expected top/front symbols to be built\n";
    return 1;
  }

  const symbols::Symbol2D &topSymbol = symbolsBuilt[0];
  const symbols::Symbol2D &frontSymbol = symbolsBuilt[1];

  const float topWidth = SymbolWidth(topSymbol);
  const float topHeight = SymbolHeight(topSymbol);
  const float frontWidth = SymbolWidth(frontSymbol);
  const float frontHeight = SymbolHeight(frontSymbol);

  if (!Near(topWidth, 1.0f) || !Near(topHeight, 0.4f)) {
    std::cerr << "Top symbol should be expressed in world units\n";
    return 1;
  }

  if (!Near(frontWidth, 1.0f) || !Near(frontHeight, 0.4f)) {
    std::cerr << "Front symbol should preserve world proportions despite viewport occupancy\n";
    return 1;
  }

  std::string svg;
  std::string error;
  if (!symbol_preview::ExportSymbolToSvgString(topSymbol, svg, error)) {
    std::cerr << "SVG export failed: " << error << "\n";
    return 1;
  }

  if (svg.find("viewBox=\"0 0 1 0.4\"") == std::string::npos) {
    std::cerr << "SVG viewBox should use normalized physical bounds\n";
    return 1;
  }

  return 0;
}
