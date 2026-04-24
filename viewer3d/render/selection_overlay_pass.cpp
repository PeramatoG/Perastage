#include "selection_overlay_pass.h"

#include "configmanager.h"
#include "opaque_pass_utils.h"
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
  const std::string &highlightUuid = controller.GetHighlightUuid();

  auto appendIfTarget = [&](const std::string &uuid,
                            std::vector<std::string> &output) {
    if (controller.IsUuidSelected(uuid) ||
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
    controller.SetupBasicLighting(context.useAmbientOcclusion,
                                  context.ambientOcclusionStrength,
                                  context.whiteModelStyle);
  else
    glDisable(GL_LIGHTING);

  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);
  glDepthMask(GL_FALSE);

  auto getTypeColor = [&](const std::string &key, const std::string &hex) {
    std::array<float, 3> c;
    if (!hex.empty() && HexToRGB(hex, c[0], c[1], c[2])) {
      controller.m_typeColors[key] = c;
      return c;
    }
    c = MakeDeterministicColor("type:" + key);
    controller.m_typeColors[key] = c;
    return c;
  };
  auto getLayerColor = [&](const std::string &key) {
    std::array<float, 3> c;
    auto opt = ConfigManager::Get().GetLayerColor(key);
    if (opt && HexToRGB(*opt, c[0], c[1], c[2])) {
      controller.m_layerColors[key] = c;
      return c;
    }
    c = MakeDeterministicColor("layer:" + key);
    controller.m_layerColors[key] = c;
    return c;
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
