#pragma once

#include "../interfaces/irendercontext.h"
#include "../mesh.h"
#include "../viewer3d_types.h"
#include <functional>

class SceneRenderer {
public:
  explicit SceneRenderer(IRenderContext &controller) : m_controller(controller) {}

  void DrawMeshWithOutline(
      const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
      bool selected, float cx, float cy, float cz, bool wireframe,
      Viewer2DRenderMode mode,
      const std::function<std::array<float, 3>(const std::array<float, 3> &)> &captureTransform,
      bool unlit, const float *modelMatrix);
  void DrawMeshWireframe(
      const Mesh &mesh, float scale,
      const std::function<std::array<float, 3>(const std::array<float, 3> &)> &captureTransform);
  void DrawMesh(const Mesh &mesh, float scale, const float *modelMatrix);
  void DrawGrid(int style, float r, float g, float b, Viewer2DView view);
  void SetupMaterialFromRGB(float r, float g, float b);

private:
  IRenderContext &m_controller;
};
