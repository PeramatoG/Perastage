#pragma once

#include "viewer3dcontroller.h"

namespace viewer2d {

struct RulerOverlayViewState {
  int width = 0;
  int height = 0;
  float zoom = 1.0f;
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  Viewer2DView view = Viewer2DView::Top;
};

void DrawRulerOverlay(const RulerOverlayViewState &state, bool darkMode);

} // namespace viewer2d
