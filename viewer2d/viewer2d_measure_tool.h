#pragma once

#include <array>
#include <optional>
#include <string>

#include "../viewer3d/viewer3d_types.h"

// Stores the interaction state for the 2D center-to-center measurement tool.
struct Viewer2DMeasureToolState {
  bool enabled = false;
  bool hasAnchor = false;
  std::string anchorUuid;
  std::array<float, 3> anchorWorld{0.0f, 0.0f, 0.0f};
  bool hasCommittedTarget = false;
  std::array<float, 3> committedTargetWorld{0.0f, 0.0f, 0.0f};
};

// Clears the current measure and keeps the enabled/disabled tool state untouched.
void ResetViewer2DMeasure(Viewer2DMeasureToolState &state);

// Converts world coordinates to viewport screen pixels for overlay rendering.
std::optional<std::array<float, 2>> Viewer2DMeasureWorldToScreen(
    const std::array<float, 3> &world, Viewer2DView view, int width, int height,
    float zoom, float offsetXPixels, float offsetYPixels);
