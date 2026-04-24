#pragma once

#include "viewer3d_types.h"

class Viewer3DController;

class SelectionOverlayPass {
public:
  static void Render(Viewer3DController &controller,
                     const RenderFrameContext &context,
                     const Viewer3DVisibleSet &visibleSet);
};
