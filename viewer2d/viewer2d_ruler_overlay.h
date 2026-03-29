#pragma once

#include "canvas2d.h"
#include "viewer3dcontroller.h"

namespace viewer2d {

struct RulerOverlayViewState {
  int width = 0;
  int height = 0;
  float zoom = 1.0f;
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  float smallTickMeters = 0.1f;
  float largeTickMeters = 0.2f;
  float xRulerPositionMeters = 0.0f;
  float yRulerPositionMeters = 0.0f;
  float zRulerPositionMeters = 0.0f;
  bool useImperialUnits = false;
  Viewer2DView view = Viewer2DView::Top;
};

struct RulerScreenLabel {
  float xPixels = 0.0f;
  float yPixels = 0.0f;
  std::string text;
  bool centerOnX = false;
  bool centerOnY = false;
};

void DrawRulerOverlay(const RulerOverlayViewState &state, bool darkMode);
void EmitRulerToCanvas(const RulerOverlayViewState &state, bool darkMode,
                       ICanvas2D &canvas);
std::vector<RulerScreenLabel>
BuildRulerScreenLabels(const RulerOverlayViewState &state);

} // namespace viewer2d
