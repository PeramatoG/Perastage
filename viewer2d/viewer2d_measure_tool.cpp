#include "viewer2d_measure_tool.h"
#include "viewer2d_coordinate_math.h"

// Resets the active and committed measurement points while preserving
// enablement.
void ResetViewer2DMeasure(Viewer2DMeasureToolState &state) {
  state.hasAnchor = false;
  state.anchorUuid.clear();
  state.hasCommittedTarget = false;
}

// Projects a 3D world position into 2D viewport pixels based on the active
// orthographic view.
std::optional<std::array<float, 2>> Viewer2DMeasureWorldToScreen(
    const std::array<float, 3> &world, Viewer2DView view, int width, int height,
    float zoom, float offsetXPixels, float offsetYPixels) {
  return viewer2d::WorldToFramebuffer(
      world, {width, height, zoom, offsetXPixels, offsetYPixels, view});
}
