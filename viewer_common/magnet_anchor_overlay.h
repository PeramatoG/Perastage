#pragma once

#include <vector>

namespace viewer_common {

struct MagnetAnchorScreenReference {
  float x = 0.0f;
  float y = 0.0f;
  float directionX = 0.0f;
  float directionY = 0.0f;
  bool hasDirection = false;
};

// Draws all Magnet anchor references in framebuffer coordinates.
void DrawMagnetAnchorOverlay(
    const std::vector<MagnetAnchorScreenReference> &references,
    int framebufferWidth, int framebufferHeight);

} // namespace viewer_common
