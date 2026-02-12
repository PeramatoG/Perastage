#pragma once

#include <array>
#include <functional>

#include "viewer3d_types.h"

class Viewer3DController;

class OpaqueFixturePass {
public:
  static void Render(
      Viewer3DController &controller, const RenderFrameContext &context,
      const Viewer3DVisibleSet &visibleSet,
      const std::function<std::array<float, 3>(const std::string &, const std::string &)> &getTypeColor,
      const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
      const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView);
};
