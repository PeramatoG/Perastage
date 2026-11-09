/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <optional>
namespace dmx_patch_ui {
// Coordinates are unscrolled canvas pixels. Semantic channels remain 1-based.
struct GridGeometry {
  int columns = 16, width = 68, height = 62;
  std::optional<int> Hit(int x, int y) const {
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x >= columns * width || y >= (512 / columns) * height) return {};
    return (y / height) * columns + x / width + 1;
  }
};
}
