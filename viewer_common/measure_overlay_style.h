#pragma once

namespace viewer_common {

// Draws a CAD-like measure overlay in the current orthographic screen-space GL context.
void DrawMeasureOverlayStyle(float x0, float y0, float x1, float y1,
                             bool darkMode);

} // namespace viewer_common
