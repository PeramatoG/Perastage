#pragma once

#include "viewer3d_types.h"

class Viewer3DController;

// Returns whether a post-base selection overlay is needed for the render style.
inline bool ShouldRenderSelectionOverlay(bool sketchPostProcess, bool wireframe,
                                         bool idOnlyPass) {
  return !idOnlyPass && (sketchPostProcess || wireframe);
}

// Returns whether the overlay should replace the muted Sketch lighting profile.
inline bool ShouldUseStandardSelectionLighting(bool sketchPostProcess) {
  return sketchPostProcess;
}

class SelectionOverlayPass {
public:
  static void Render(Viewer3DController &controller,
                     const RenderFrameContext &context,
                     const Viewer3DVisibleSet &visibleSet);
};
