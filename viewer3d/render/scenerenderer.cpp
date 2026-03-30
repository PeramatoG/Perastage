#include "scenerenderer.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cmath>

namespace {
struct InkColor {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
};

struct LineRenderProfile {
  float lineWidth = 1.0f;
  bool enableLineSmoothing = false;
};

LineRenderProfile GetLineRenderProfile(bool isInteracting, bool wireframeMode,
                                       bool adaptiveEnabled) {
  if (!adaptiveEnabled)
    return {wireframeMode ? 1.0f : 2.0f, false};
  if (isInteracting)
    return {1.0f, false};
  return {wireframeMode ? 1.0f : 2.0f, true};
}

std::array<float, 3> NormalizeVector(float x, float y, float z) {
  const float length = std::sqrt(x * x + y * y + z * z);
  if (length <= 1e-6f)
    return {0.0f, 0.0f, 1.0f};
  return {x / length, y / length, z / length};
}

std::array<float, 3> TransformNormal(const std::array<float, 3> &n,
                                     const float *modelMatrix) {
  if (!modelMatrix)
    return n;
  const float x = modelMatrix[0] * n[0] + modelMatrix[4] * n[1] +
                  modelMatrix[8] * n[2];
  const float y = modelMatrix[1] * n[0] + modelMatrix[5] * n[1] +
                  modelMatrix[9] * n[2];
  const float z = modelMatrix[2] * n[0] + modelMatrix[6] * n[1] +
                  modelMatrix[10] * n[2];
  return NormalizeVector(x, y, z);
}

InkColor QuantizeInkTone(float diffuseFactor) {
  // 3-ink white-model palette with lighting weight:
  // white 70%, light gray 20%, dark gray 10%.
  static constexpr float kDarkThreshold = 0.10f;
  static constexpr float kLightThreshold = 0.30f;
  if (diffuseFactor <= kDarkThreshold)
    return {0.62f, 0.62f, 0.62f};
  if (diffuseFactor <= kLightThreshold)
    return {0.84f, 0.84f, 0.84f};
  return {1.0f, 1.0f, 1.0f};
}

void DrawMeshThreeToneInk(const Mesh &mesh, float scale, const float *modelMatrix) {
  std::array<float, 3> lightDir = NormalizeVector(0.35f, -0.55f, 1.0f);
  const bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  const bool flipWinding =
      (modelMatrix != nullptr) && TransformDeterminant(modelMatrix) < 0.0f;

  const std::vector<unsigned short> *triangleIndices = &mesh.indices;
  if (flipWinding) {
    if (mesh.flippedIndicesCache.size() != mesh.indices.size()) {
      mesh.flippedIndicesCache = mesh.indices;
      for (size_t i = 0; i + 2 < mesh.flippedIndicesCache.size(); i += 3)
        std::swap(mesh.flippedIndicesCache[i + 1], mesh.flippedIndicesCache[i + 2]);
    }
    triangleIndices = &mesh.flippedIndicesCache;
  }

  const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
  if (cullWasEnabled)
    glDisable(GL_CULL_FACE);
  glShadeModel(GL_SMOOTH);

  glBegin(GL_TRIANGLES);
  for (size_t i = 0; i + 2 < triangleIndices->size(); i += 3) {
    const unsigned short tri[3] = {(*triangleIndices)[i], (*triangleIndices)[i + 1],
                                   (*triangleIndices)[i + 2]};
    for (int v = 0; v < 3; ++v) {
      const unsigned short idx = tri[v];
      const float vx = mesh.vertices[idx * 3] * scale;
      const float vy = mesh.vertices[idx * 3 + 1] * scale;
      const float vz = mesh.vertices[idx * 3 + 2] * scale;

      std::array<float, 3> n = {0.0f, 0.0f, 1.0f};
      if (hasNormals) {
        n = NormalizeVector(mesh.normals[idx * 3], mesh.normals[idx * 3 + 1],
                            mesh.normals[idx * 3 + 2]);
      }
      n = TransformNormal(n, modelMatrix);
      const float diffuse = std::max(
          0.0f, n[0] * lightDir[0] + n[1] * lightDir[1] + n[2] * lightDir[2]);
      const InkColor tone = QuantizeInkTone(diffuse);
      glColor3f(tone.r, tone.g, tone.b);
      glNormal3f(n[0], n[1], n[2]);
      glVertex3f(vx, vy, vz);
    }
  }
  glEnd();

  if (cullWasEnabled)
    glEnable(GL_CULL_FACE);
}
} // namespace

void SceneRenderer::DrawMeshWithOutline(
    const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
    bool selected, float cx, float cy, float cz, bool wireframe,
    Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform,
    bool unlit, const float *modelMatrix) {
  (void)cx;
  (void)cy;
  (void)cz;

  if (wireframe) {
    float lineWidth =
        GetLineRenderProfile(m_controller.IsInteracting(),
                             mode == Viewer2DRenderMode::Wireframe,
                             m_controller.UseAdaptiveLineProfile())
            .lineWidth;
    const bool drawOutline =
        !m_controller.SkipOutlinesForCurrentFrame() &&
        m_controller.IsSelectionOutlineEnabled2D() && (highlight || selected);
    if (!m_controller.IsCaptureOnly()) {
      if (drawOutline) {
        float glowWidth = lineWidth + 3.0f;
        glLineWidth(glowWidth);
        if (highlight)
          m_controller.SetGLColor(0.0f, 1.0f, 0.0f);
        else if (selected)
          m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
        DrawMeshWireframe(mesh, scale, captureTransform);
      }
      glLineWidth(lineWidth);
      m_controller.SetGLColor(0.0f, 0.0f, 0.0f);
    }
    CanvasStroke stroke;
    stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
    stroke.width = lineWidth;
    DrawMeshWireframe(mesh, scale, captureTransform);
    if (m_controller.GetCaptureCanvas() && mode != Viewer2DRenderMode::Wireframe) {
      CanvasFill fill;
      fill.color = {r, g, b, 1.0f};
      for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        unsigned short i0 = mesh.indices[i];
        unsigned short i1 = mesh.indices[i + 1];
        unsigned short i2 = mesh.indices[i + 2];
        std::vector<std::array<float, 3>> pts = {
            {mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
             mesh.vertices[i0 * 3 + 2] * scale},
            {mesh.vertices[i1 * 3] * scale, mesh.vertices[i1 * 3 + 1] * scale,
             mesh.vertices[i1 * 3 + 2] * scale},
            {mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
             mesh.vertices[i2 * 3 + 2] * scale}};
        if (captureTransform) {
          for (auto &p : pts)
            p = captureTransform(p);
        }
        m_controller.RecordPolygon(pts, stroke, &fill);
      }
    }
    if (!m_controller.IsCaptureOnly()) {
      glLineWidth(1.0f);
      if (mode != Viewer2DRenderMode::Wireframe) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        m_controller.SetGLColor(r, g, b);
        if (unlit)
          glDisable(GL_LIGHTING);
        DrawMesh(mesh, scale, modelMatrix);
        if (unlit)
          glEnable(GL_LIGHTING);
        glDisable(GL_POLYGON_OFFSET_FILL);
      }
    }
    return;
  }

  if (!m_controller.IsCaptureOnly()) {
    if (m_controller.IsWhiteModelStyleEnabled()) {
      // Keep 3D white-model aligned with the 2D viewer draw order:
      // stroke pass first, then polygon-offset fill pass.
      const float lineWidth = 2.6f;
      auto setHighlightOrSelectionColor = [&]() {
        if (highlight)
          m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
        else if (selected)
          m_controller.SetGLColor(0.15f, 0.35f, 1.0f);
        else
          m_controller.SetGLColor(0.0f, 0.0f, 0.0f);
      };
      const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
      const GLboolean lineSmoothWasEnabled = glIsEnabled(GL_LINE_SMOOTH);
      if (lightingWasEnabled)
        glDisable(GL_LIGHTING);
      glDisable(GL_LINE_SMOOTH);
      glLineWidth(lineWidth);
      setHighlightOrSelectionColor();
      DrawMeshWireframe(mesh, scale, captureTransform);
      glLineWidth(1.0f);
      if (lineSmoothWasEnabled)
        glEnable(GL_LINE_SMOOTH);
      if (lightingWasEnabled)
        glEnable(GL_LIGHTING);

      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(-1.0f, -1.0f);
      const GLboolean fillLightingWasEnabled = glIsEnabled(GL_LIGHTING);
      if (fillLightingWasEnabled)
        glDisable(GL_LIGHTING);
      if (highlight || selected) {
        setHighlightOrSelectionColor();
        DrawMesh(mesh, scale, modelMatrix);
      } else {
        DrawMeshThreeToneInk(mesh, scale, modelMatrix);
      }
      if (fillLightingWasEnabled)
        glEnable(GL_LIGHTING);
      glDisable(GL_POLYGON_OFFSET_FILL);
    } else {
      if (highlight)
        m_controller.SetGLColor(0.0f, 1.0f, 0.0f);
      else if (selected)
        m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
      else
        m_controller.SetGLColor(r, g, b);

      if (unlit)
        glDisable(GL_LIGHTING);
      DrawMesh(mesh, scale, modelMatrix);
      if (unlit)
        glEnable(GL_LIGHTING);
    }
  }
  if (m_controller.GetCaptureCanvas()) {
    CanvasStroke stroke;
    stroke.color = {r, g, b, 1.0f};
    stroke.width = 0.0f;
    CanvasFill fill;
    fill.color = {r, g, b, 1.0f};
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      unsigned short i0 = mesh.indices[i];
      unsigned short i1 = mesh.indices[i + 1];
      unsigned short i2 = mesh.indices[i + 2];
      std::vector<std::array<float, 3>> pts = {
          {mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
           mesh.vertices[i0 * 3 + 2] * scale},
          {mesh.vertices[i1 * 3] * scale, mesh.vertices[i1 * 3 + 1] * scale,
           mesh.vertices[i1 * 3 + 2] * scale},
          {mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
           mesh.vertices[i2 * 3 + 2] * scale}};
      if (captureTransform) {
        for (auto &p : pts)
          p = captureTransform(p);
      }
      m_controller.RecordPolygon(pts, stroke, &fill);
    }
  }
}

void SceneRenderer::DrawMeshWireframe(
    const Mesh &mesh, float scale,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  const bool gpuHandlesValid = glIsBuffer(mesh.vboVertices) == GL_TRUE &&
                               glIsBuffer(mesh.eboLines) == GL_TRUE &&
                               glIsBuffer(mesh.eboTriangles) == GL_TRUE;
  const bool canUseGpuWireframe =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboVertices != 0 &&
      mesh.eboLines != 0 && mesh.eboTriangles != 0 && gpuHandlesValid;

  if (!m_controller.IsCaptureOnly() && canUseGpuWireframe) {
    glBindVertexArray(mesh.vao);
    glPushMatrix();
    glScalef(scale, scale, scale);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vboVertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboLines);
    glDrawElements(GL_LINES, mesh.lineIndexCount, GL_UNSIGNED_SHORT, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);

    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
  } else if (!m_controller.IsCaptureOnly()) {
    glBegin(GL_LINES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      const unsigned short i0 = mesh.indices[i];
      const unsigned short i1 = mesh.indices[i + 1];
      const unsigned short i2 = mesh.indices[i + 2];

      glVertex3f(mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
                 mesh.vertices[i0 * 3 + 2] * scale);
      glVertex3f(mesh.vertices[i1 * 3] * scale, mesh.vertices[i1 * 3 + 1] * scale,
                 mesh.vertices[i1 * 3 + 2] * scale);

      glVertex3f(mesh.vertices[i1 * 3] * scale, mesh.vertices[i1 * 3 + 1] * scale,
                 mesh.vertices[i1 * 3 + 2] * scale);
      glVertex3f(mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
                 mesh.vertices[i2 * 3 + 2] * scale);

      glVertex3f(mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
                 mesh.vertices[i2 * 3 + 2] * scale);
      glVertex3f(mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
                 mesh.vertices[i0 * 3 + 2] * scale);
    }
    glEnd();
  }
  if (m_controller.GetCaptureCanvas()) {
    CanvasStroke stroke;
    stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
    stroke.width = 1.0f;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      unsigned short i0 = mesh.indices[i];
      unsigned short i1 = mesh.indices[i + 1];
      unsigned short i2 = mesh.indices[i + 2];

      std::array<float, 3> p0 = {mesh.vertices[i0 * 3] * scale,
                                 mesh.vertices[i0 * 3 + 1] * scale,
                                 mesh.vertices[i0 * 3 + 2] * scale};
      std::array<float, 3> p1 = {mesh.vertices[i1 * 3] * scale,
                                 mesh.vertices[i1 * 3 + 1] * scale,
                                 mesh.vertices[i1 * 3 + 2] * scale};
      std::array<float, 3> p2 = {mesh.vertices[i2 * 3] * scale,
                                 mesh.vertices[i2 * 3 + 1] * scale,
                                 mesh.vertices[i2 * 3 + 2] * scale};
      if (captureTransform) {
        p0 = captureTransform(p0);
        p1 = captureTransform(p1);
        p2 = captureTransform(p2);
      }
      m_controller.RecordLine(p0, p1, stroke);
      m_controller.RecordLine(p1, p2, stroke);
      m_controller.RecordLine(p2, p0, stroke);
    }
  }
}

void SceneRenderer::DrawMesh(const Mesh &mesh, float scale, const float *modelMatrix) {
  const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
  if (cullWasEnabled)
    glDisable(GL_CULL_FACE);

  const bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  const bool flipWinding =
      (modelMatrix != nullptr) && TransformDeterminant(modelMatrix) < 0.0f;

  const std::vector<unsigned short> *triangleIndices = &mesh.indices;
  if (flipWinding) {
    if (mesh.flippedIndicesCache.size() != mesh.indices.size()) {
      mesh.flippedIndicesCache = mesh.indices;
      for (size_t i = 0; i + 2 < mesh.flippedIndicesCache.size(); i += 3) {
        std::swap(mesh.flippedIndicesCache[i + 1],
                  mesh.flippedIndicesCache[i + 2]);
      }
    }
    triangleIndices = &mesh.flippedIndicesCache;
  }

  const bool gpuHandlesValid = glIsBuffer(mesh.vboVertices) == GL_TRUE &&
                               glIsBuffer(mesh.vboNormals) == GL_TRUE &&
                               glIsBuffer(mesh.eboTriangles) == GL_TRUE;
  const bool requiresCpuDrawPath = flipWinding;
  const bool canUseGpuTriangles =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboVertices != 0 &&
      mesh.vboNormals != 0 && mesh.eboTriangles != 0 && gpuHandlesValid &&
      !requiresCpuDrawPath;

  if (!m_controller.IsCaptureOnly() && canUseGpuTriangles) {
    glBindVertexArray(mesh.vao);
    glPushMatrix();
    glScalef(scale, scale, scale);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vboVertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);
    glDrawElements(GL_TRIANGLES, mesh.triangleIndexCount, GL_UNSIGNED_SHORT,
                   nullptr);

    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
  } else if (!m_controller.IsCaptureOnly()) {
    GLint shadeModel = GL_SMOOTH;
    glGetIntegerv(GL_SHADE_MODEL, &shadeModel);
    const bool useFaceNormals = (shadeModel == GL_FLAT);

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i + 2 < triangleIndices->size(); i += 3) {
      const unsigned short i0 = (*triangleIndices)[i];
      const unsigned short i1 = (*triangleIndices)[i + 1];
      const unsigned short i2 = (*triangleIndices)[i + 2];

      const float v0x = mesh.vertices[i0 * 3] * scale;
      const float v0y = mesh.vertices[i0 * 3 + 1] * scale;
      const float v0z = mesh.vertices[i0 * 3 + 2] * scale;
      const float v1x = mesh.vertices[i1 * 3] * scale;
      const float v1y = mesh.vertices[i1 * 3 + 1] * scale;
      const float v1z = mesh.vertices[i1 * 3 + 2] * scale;
      const float v2x = mesh.vertices[i2 * 3] * scale;
      const float v2y = mesh.vertices[i2 * 3 + 1] * scale;
      const float v2z = mesh.vertices[i2 * 3 + 2] * scale;

      const auto &normalData = mesh.normals;

      if (useFaceNormals) {
        if (hasNormals) {
          float ax = normalData[i0 * 3] + normalData[i1 * 3] +
                     normalData[i2 * 3];
          float ay = normalData[i0 * 3 + 1] + normalData[i1 * 3 + 1] +
                     normalData[i2 * 3 + 1];
          float az = normalData[i0 * 3 + 2] + normalData[i1 * 3 + 2] +
                     normalData[i2 * 3 + 2];
          const float alen = std::sqrt(ax * ax + ay * ay + az * az);
          if (alen > 0.0f) {
            glNormal3f(ax / alen, ay / alen, az / alen);
          } else {
            glNormal3f(0.0f, 0.0f, 1.0f);
          }
        } else {
          float nx = (v1y - v0y) * (v2z - v0z) - (v1z - v0z) * (v2y - v0y);
          float ny = (v1z - v0z) * (v2x - v0x) - (v1x - v0x) * (v2z - v0z);
          float nz = (v1x - v0x) * (v2y - v0y) - (v1y - v0y) * (v2x - v0x);
          const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
          if (len > 0.0f) {
            glNormal3f(nx / len, ny / len, nz / len);
          } else {
            glNormal3f(0.0f, 0.0f, 1.0f);
          }
        }

        glVertex3f(v0x, v0y, v0z);
        glVertex3f(v1x, v1y, v1z);
        glVertex3f(v2x, v2y, v2z);
        continue;
      }

      if (hasNormals) {
        glNormal3f(normalData[i0 * 3], normalData[i0 * 3 + 1],
                   normalData[i0 * 3 + 2]);
        glVertex3f(v0x, v0y, v0z);
        glNormal3f(normalData[i1 * 3], normalData[i1 * 3 + 1],
                   normalData[i1 * 3 + 2]);
        glVertex3f(v1x, v1y, v1z);
        glNormal3f(normalData[i2 * 3], normalData[i2 * 3 + 1],
                   normalData[i2 * 3 + 2]);
        glVertex3f(v2x, v2y, v2z);
      } else {
        float nx = (v1y - v0y) * (v2z - v0z) - (v1z - v0z) * (v2y - v0y);
        float ny = (v1z - v0z) * (v2x - v0x) - (v1x - v0x) * (v2z - v0z);
        float nz = (v1x - v0x) * (v2y - v0y) - (v1y - v0y) * (v2x - v0x);
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) {
          nx /= len;
          ny /= len;
          nz /= len;
        }

        glNormal3f(nx, ny, nz);
        glVertex3f(v0x, v0y, v0z);
        glVertex3f(v1x, v1y, v1z);
        glVertex3f(v2x, v2y, v2z);
      }
    }
    glEnd();
  }

  if (cullWasEnabled)
    glEnable(GL_CULL_FACE);
}

void SceneRenderer::DrawGrid(int style, float r, float g, float b,
                             Viewer2DView view) {
  const float size = 20.0f;
  const float step = 1.0f;

  const LineRenderProfile profile =
      GetLineRenderProfile(m_controller.IsInteracting(), true,
                           m_controller.UseAdaptiveLineProfile());
  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = profile.lineWidth;

  const GLboolean lineSmoothWasEnabled = glIsEnabled(GL_LINE_SMOOTH);
  if (profile.enableLineSmoothing)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);

  m_controller.SetGLColor(r, g, b);
  if (style == 0) {
    glLineWidth(profile.lineWidth);
    glBegin(GL_LINES);
    for (float i = -size; i <= size; i += step) {
      switch (view) {
      case Viewer2DView::Top:
      case Viewer2DView::Bottom:
        glVertex3f(i, -size, 0.0f);
        glVertex3f(i, size, 0.0f);
        glVertex3f(-size, i, 0.0f);
        glVertex3f(size, i, 0.0f);
        if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid()) {
          m_controller.RecordLine({i, -size, 0.0f}, {i, size, 0.0f}, stroke);
          m_controller.RecordLine({-size, i, 0.0f}, {size, i, 0.0f}, stroke);
        }
        break;
      case Viewer2DView::Front:
        glVertex3f(i, 0.0f, -size);
        glVertex3f(i, 0.0f, size);
        glVertex3f(-size, 0.0f, i);
        glVertex3f(size, 0.0f, i);
        if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid()) {
          m_controller.RecordLine({i, 0.0f, -size}, {i, 0.0f, size}, stroke);
          m_controller.RecordLine({-size, 0.0f, i}, {size, 0.0f, i}, stroke);
        }
        break;
      case Viewer2DView::Side:
        glVertex3f(0.0f, i, -size);
        glVertex3f(0.0f, i, size);
        glVertex3f(0.0f, -size, i);
        glVertex3f(0.0f, size, i);
        if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid()) {
          m_controller.RecordLine({0.0f, i, -size}, {0.0f, i, size}, stroke);
          m_controller.RecordLine({0.0f, -size, i}, {0.0f, size, i}, stroke);
        }
        break;
      }
    }
    glEnd();
  } else if (style == 1) {
    GLboolean pointSmooth = glIsEnabled(GL_POINT_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        switch (view) {
        case Viewer2DView::Top:
        case Viewer2DView::Bottom:
          glVertex3f(x, y, 0.0f);
          if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid())
            m_controller.RecordLine({x, y, 0.0f}, {x, y, 0.0f}, stroke);
          break;
        case Viewer2DView::Front:
          glVertex3f(x, 0.0f, y);
          if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid())
            m_controller.RecordLine({x, 0.0f, y}, {x, 0.0f, y}, stroke);
          break;
        case Viewer2DView::Side:
          glVertex3f(0.0f, x, y);
          if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid())
            m_controller.RecordLine({0.0f, x, y}, {0.0f, x, y}, stroke);
          break;
        }
      }
    }
    glEnd();
    if (pointSmooth)
      glEnable(GL_POINT_SMOOTH);
  } else {
    float half = step * 0.1f;
    glLineWidth(profile.lineWidth);
    glBegin(GL_LINES);
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        switch (view) {
        case Viewer2DView::Top:
        case Viewer2DView::Bottom:
          glVertex3f(x - half, y, 0.0f);
          glVertex3f(x + half, y, 0.0f);
          glVertex3f(x, y - half, 0.0f);
          glVertex3f(x, y + half, 0.0f);
          if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid()) {
            m_controller.RecordLine({x - half, y, 0.0f}, {x + half, y, 0.0f}, stroke);
            m_controller.RecordLine({x, y - half, 0.0f}, {x, y + half, 0.0f}, stroke);
          }
          break;
        case Viewer2DView::Front:
          glVertex3f(x - half, 0.0f, y);
          glVertex3f(x + half, 0.0f, y);
          glVertex3f(x, 0.0f, y - half);
          glVertex3f(x, 0.0f, y + half);
          if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid()) {
            m_controller.RecordLine({x - half, 0.0f, y}, {x + half, 0.0f, y}, stroke);
            m_controller.RecordLine({x, 0.0f, y - half}, {x, 0.0f, y + half}, stroke);
          }
          break;
        case Viewer2DView::Side:
          glVertex3f(0.0f, x - half, y);
          glVertex3f(0.0f, x + half, y);
          glVertex3f(0.0f, x, y - half);
          glVertex3f(0.0f, x, y + half);
          if (m_controller.GetCaptureCanvas() && m_controller.CaptureIncludesGrid()) {
            m_controller.RecordLine({0.0f, x - half, y}, {0.0f, x + half, y}, stroke);
            m_controller.RecordLine({0.0f, x, y - half}, {0.0f, x, y + half}, stroke);
          }
          break;
        }
      }
    }
    glEnd();
  }

  if (lineSmoothWasEnabled)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);
}

void SceneRenderer::SetupMaterialFromRGB(float r, float g, float b) {
  m_controller.SetGLColor(r, g, b);
}
