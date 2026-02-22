#pragma once

#include <vector>

#include "Symbol2D.h"

namespace symbols {

struct RenderedSymbolImage {
  SymbolView view = SymbolView::Top;
  int width = 0;
  int height = 0;
  float worldUnitsPerPixel = 1.0f;
  std::vector<unsigned char> rgba;
};

struct ImageBuildParams {
  float previewStrokeWidthPx = 2.0f;
  unsigned char fillAlphaThreshold = 10;
  unsigned char backgroundTolerance = 18;
  unsigned char blackThreshold = 80;
  int minStrokePixels = 6;
};

class Symbol2DImageBuilder {
public:
  static std::vector<Symbol2D>
  BuildFromRenderedImages(const std::vector<RenderedSymbolImage> &renders,
                          const ImageBuildParams &params = ImageBuildParams{});
};

} // namespace symbols
