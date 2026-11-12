/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <optional>
namespace dmx_patch_ui {
// Coordinates are unscrolled canvas pixels. Semantic channels remain 1-based.
struct GridGeometry {
  int columns = 16, width = 52, height = 46;
  std::optional<int> Hit(int x, int y) const {
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x >= columns * width || y >= ((512 + columns - 1) / columns) * height) return {};
    const int channel = (y / height) * columns + x / width + 1;
    return channel <= 512 ? std::optional<int>(channel) : std::nullopt;
  }
};
}
