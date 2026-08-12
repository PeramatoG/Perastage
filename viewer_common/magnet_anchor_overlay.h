#pragma once

namespace viewer_common {

// Draws paired Magnet anchor references in framebuffer coordinates.
void DrawMagnetAnchorOverlay(float sourceX, float sourceY, float targetX,
                             float targetY, int framebufferWidth,
                             int framebufferHeight, bool darkMode);

} // namespace viewer_common
