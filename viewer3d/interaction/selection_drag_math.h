#pragma once

#include <array>

namespace viewer3d {

enum class SelectionDragAxis { None, X, Y, Z };

struct ProjectedAxis {
  double screenDx = 0.0;
  double screenDy = 0.0;
  double pixelsPerMeter = 0.0;
  bool valid = false;
};

SelectionDragAxis SelectDragAxisFromMouseDelta(
    int mouseDx, int mouseDy, const std::array<ProjectedAxis, 3> &axes,
    int activationThresholdPx = 3);

double ComputeDragMetersOnAxis(int mouseDx, int mouseDy, SelectionDragAxis axis,
                               const std::array<ProjectedAxis, 3> &axes);

} // namespace viewer3d
