#pragma once

#include "irendercontext.h"
#include "lighting_profile.h"
#include "mesh.h"
#include "viewer3d_types.h"
#include <functional>

class SceneRenderer {
public:
  // Initializes a scene renderer for the supplied rendering context.
  explicit SceneRenderer(IRenderContext &controller)
      : m_controller(controller) {
    m_sketchLightingState.keyLightEyeDirection =
        Viewer3DLightingProfile::NormalizeDirection(
            Viewer3DLightingProfile::kKeyLightWorldDirection);
    m_sketchLightingState.fillLightEyeDirection =
        Viewer3DLightingProfile::NormalizeDirection(
            Viewer3DLightingProfile::kFillLightWorldDirection);
  }

  void SetSketchLightingState(
      const Viewer3DLightingProfile::LightingState &lightingState);

  void DrawMeshWithOutline(
      const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
      bool groupHighlight, bool selected, float cx, float cy, float cz,
      bool wireframe, Viewer2DRenderMode mode,
      const std::function<std::array<float, 3>(const std::array<float, 3> &)>
          &captureTransform,
      bool unlit, const float *modelMatrix, bool disableDepthBias = false);
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
  Viewer3DLightingProfile::LightingState m_sketchLightingState;
};
