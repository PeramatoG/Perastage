#pragma once

#include <array>
#include <functional>

#include "viewer3d_types.h"
#include "symbolcache.h"

class Viewer3DController;

class OpaqueObjectPass {
public:
  static void Render(
      Viewer3DController &controller, const RenderFrameContext &context,
      const Viewer3DVisibleSet &visibleSet,
      const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
      const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView);
};
