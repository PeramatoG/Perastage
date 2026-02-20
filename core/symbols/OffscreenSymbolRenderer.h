#pragma once

#include "symbols/Symbol2DTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace symbols {

enum class RenderMode { ShapeBlack, LinesBlackOnWhiteFill };

struct ImageRGBA {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;

  uint8_t *Pixel(int x, int y) { return &pixels[(y * width + x) * 4]; }
  const uint8_t *Pixel(int x, int y) const { return &pixels[(y * width + x) * 4]; }
};

struct RenderResult {
  ImageRGBA rgba;
  std::vector<float> depth;
};

class OffscreenSymbolRenderer {
public:
  RenderResult RenderFixtureTechnical(const std::string &gdtfSpec,
                                      SymbolView view,
                                      int width,
                                      int height,
                                      RenderMode mode) const;
};

} // namespace symbols
