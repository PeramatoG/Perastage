#include "scenerenderer.h"
#include "../viewer3dcontroller.h"

void SceneRenderer::DrawMeshWithOutline(
    const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
    bool selected, float cx, float cy, float cz, bool wireframe,
    Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &captureTransform,
    bool unlit, const float *modelMatrix) {
  m_controller.DrawMeshWithOutlineImpl(mesh, r, g, b, scale, highlight, selected,
                                       cx, cy, cz, wireframe, mode,
                                       captureTransform, unlit, modelMatrix);
}

void SceneRenderer::DrawMeshWireframe(
    const Mesh &mesh, float scale,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &captureTransform) {
  m_controller.DrawMeshWireframeImpl(mesh, scale, captureTransform);
}

void SceneRenderer::DrawMesh(const Mesh &mesh, float scale, const float *modelMatrix) {
  m_controller.DrawMeshImpl(mesh, scale, modelMatrix);
}

void SceneRenderer::DrawGrid(int style, float r, float g, float b, Viewer2DView view) {
  m_controller.DrawGridImpl(style, r, g, b, view);
}

void SceneRenderer::SetupMaterialFromRGB(float r, float g, float b) {
  m_controller.SetupMaterialFromRGBImpl(r, g, b);
}
