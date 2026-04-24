#include "selection_overlay_pass.h"

#include "opaque_fixture_pass.h"
#include "opaque_object_pass.h"
#include "opaque_truss_pass.h"
#include "viewer3dcontroller.h"

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <array>

namespace {
Viewer3DVisibleSet BuildOverlayVisibleSet(const Viewer3DController &controller,
                                          const Viewer3DVisibleSet &visibleSet) {
  Viewer3DVisibleSet overlayVisibleSet;
  const std::string &highlightUuid = controller.GetOverlayHighlightUuid();
  const auto &selectedUuids = controller.GetOverlaySelectedUuids();

  auto appendIfTarget = [&](const std::string &uuid,
                            std::vector<std::string> &output) {
    if (selectedUuids.find(uuid) != selectedUuids.end() ||
        (!highlightUuid.empty() && uuid == highlightUuid)) {
      output.push_back(uuid);
    }
  };

  for (const std::string &uuid : visibleSet.fixtureUuids)
    appendIfTarget(uuid, overlayVisibleSet.fixtureUuids);
  for (const std::string &uuid : visibleSet.trussUuids)
    appendIfTarget(uuid, overlayVisibleSet.trussUuids);
  for (const std::string &uuid : visibleSet.objectUuids)
    appendIfTarget(uuid, overlayVisibleSet.objectUuids);

  return overlayVisibleSet;
}
} // namespace

void SelectionOverlayPass::Render(Viewer3DController &controller,
                                  const RenderFrameContext &context,
                                  const Viewer3DVisibleSet &visibleSet) {
  const Viewer3DVisibleSet overlayVisibleSet =
      BuildOverlayVisibleSet(controller, visibleSet);
  if (overlayVisibleSet.Empty())
    return;

  if (context.useLighting)
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);

  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);
  glDepthMask(GL_FALSE);

  auto getTypeColor = [](const std::string &, const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };
  auto getLayerColor = [](const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };
  auto resolveSymbolView = [](Viewer2DView viewKind) {
    switch (viewKind) {
    case Viewer2DView::Top:
      return SymbolViewKind::Top;
    case Viewer2DView::Front:
      return SymbolViewKind::Front;
    case Viewer2DView::Side:
      return SymbolViewKind::Left;
    case Viewer2DView::Bottom:
    default:
      return SymbolViewKind::Bottom;
    }
  };
  auto getPickColor = [](const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };

  RenderFrameContext overlayContext = context;
  overlayContext.skipCapture = true;

  OpaqueObjectPass::Render(controller, overlayContext, overlayVisibleSet,
                           getLayerColor, resolveSymbolView, getPickColor);
  OpaqueTrussPass::Render(controller, overlayContext, overlayVisibleSet,
                          getLayerColor, resolveSymbolView, getPickColor);
  OpaqueFixturePass::Render(controller, overlayContext, overlayVisibleSet,
                            getTypeColor, getLayerColor, resolveSymbolView,
                            getPickColor);

  glDepthMask(depthMaskEnabled == GL_TRUE ? GL_TRUE : GL_FALSE);
}
