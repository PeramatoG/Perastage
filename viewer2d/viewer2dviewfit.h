#pragma once

#include "viewer3d_types.h"

class Viewer3DController;

namespace viewer2d {

struct ViewFitResult {
  float offsetXPixels = 0.0f;
  float offsetYPixels = 0.0f;
  float zoom = 1.0f;
};

bool ComputeViewFit(const Viewer3DController &controller, Viewer2DView view,
                    int viewportWidth, int viewportHeight,
                    ViewFitResult &result);

} // namespace viewer2d
