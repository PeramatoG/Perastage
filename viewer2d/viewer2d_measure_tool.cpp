#include "viewer2d_measure_tool.h"

namespace {
constexpr float kPixelsPerMeter = 25.0f;
}

// Resets the active and committed measurement points while preserving enablement.
void ResetViewer2DMeasure(Viewer2DMeasureToolState &state) {
  state.hasAnchor = false;
  state.anchorUuid.clear();
  state.hasCommittedTarget = false;
}

// Projects a 3D world position into 2D viewport pixels based on the active orthographic view.
std::optional<std::array<float, 2>> Viewer2DMeasureWorldToScreen(
    const std::array<float, 3> &world, Viewer2DView view, int width, int height,
    float zoom, float offsetXPixels, float offsetYPixels) {
  if (width <= 0 || height <= 0 || zoom <= 0.0f)
    return std::nullopt;
  float u = 0.0f;
  float v = 0.0f;
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    u = world[0];
    v = world[1];
    break;
  case Viewer2DView::Front:
    u = world[0];
    v = world[2];
    break;
  case Viewer2DView::Side:
    u = world[1];
    v = world[2];
    break;
  }
  const float ppm = kPixelsPerMeter * zoom;
  const float sx = width * 0.5f + (u * ppm) + offsetXPixels;
  const float sy = height * 0.5f - (v * ppm) - offsetYPixels;
  return std::array<float, 2>{sx, sy};
}
