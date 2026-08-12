#pragma once

#include "irendercontext.h"
#include "mesh.h"
#include "viewer3d_types.h"
#include <functional>

// Returns whether a highlighted fill should retain scene lighting.
inline bool ShouldLightSelectionFill(bool selectionOverlayPass) {
  return !selectionOverlayPass;
}

class SceneRenderer {
public:
  explicit SceneRenderer(IRenderContext &controller)
      : m_controller(controller) {}

  void DrawMeshWithOutline(
      const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
      bool groupHighlight, bool selected, float cx, float cy, float cz,
      bool wireframe, Viewer2DRenderMode mode,
      const std::function<std::array<float, 3>(const std::array<float, 3> &)>
          &captureTransform,
      bool unlit, const float *modelMatrix, bool disableDepthBias = false,
      bool selectionOverlayPass = false);
  void DrawMeshWireframe(
      const Mesh &mesh, float scale,
      const std::function<std::array<float, 3>(const std::array<float, 3> &)>
          &captureTransform,
      const CanvasStroke *captureStroke = nullptr, int triangleStep = 1);
  void DrawMesh(const Mesh &mesh, float scale, const float *modelMatrix,
                bool useTexture = false);
  void DrawGrid(int style, float r, float g, float b, Viewer2DView view);
  void SetupMaterialFromRGB(float r, float g, float b);

private:
  IRenderContext &m_controller;
};
