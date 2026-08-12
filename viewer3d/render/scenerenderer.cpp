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

#include "configmanager.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <wx/log.h>

namespace {
constexpr float kPrimaryHighlightR = 0.78f;
constexpr float kPrimaryHighlightG = 1.0f;
constexpr float kPrimaryHighlightB = 0.0f;
constexpr float kGroupHighlightR = 0.0f;
constexpr float kGroupHighlightG = 0.45f;
constexpr float kGroupHighlightB = 0.85f;

struct LineRenderProfile {
  float lineWidth = 1.0f;
  bool enableLineSmoothing = false;
};

enum class SketchInteractionWireframeMode {
  FullQuality = 0,
  Sparse = 1,
  FillOnly = 2,
  HighlightSelectedOnly = 3
};

// Reads the configured wireframe simplification mode used while sketch rendering is interactive.
SketchInteractionWireframeMode ReadSketchInteractionWireframeMode() {
  const float rawValue = ConfigManager::Get().GetFloat(
      "viewer3d_sketch_interaction_wireframe_mode");
  const int mode = static_cast<int>(std::lround(rawValue));
  switch (mode) {
  case 1:
    return SketchInteractionWireframeMode::Sparse;
  case 2:
    return SketchInteractionWireframeMode::FillOnly;
  case 3:
    return SketchInteractionWireframeMode::HighlightSelectedOnly;
  default:
    return SketchInteractionWireframeMode::FullQuality;
  }
}

// Reads the configured sketch wireframe triangle step and clamps it to a safe range.
int ReadSketchInteractionWireframeStep() {
  const float rawStep = ConfigManager::Get().GetFloat(
      "viewer3d_sketch_interaction_wireframe_step");
  const int step = static_cast<int>(std::lround(rawStep));
  return std::clamp(step, 1, 12);
}

// Returns line rendering settings for the current interaction and wireframe state.
LineRenderProfile GetLineRenderProfile(bool isInteracting, bool wireframeMode,
                                       bool adaptiveEnabled) {
  (void)isInteracting;
  if (!adaptiveEnabled)
    return {wireframeMode ? 1.0f : 2.0f, false};
  return {wireframeMode ? 1.0f : 2.0f, true};
}

// Restores the mesh VAO element binding and unbinds the VAO safely.
void RestoreMeshVaoElementBindingAndUnbind(const Mesh &mesh,
                                           const char *label) {
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);
#ifndef NDEBUG
  GLint currentEbo = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentEbo);
  if (static_cast<GLuint>(currentEbo) != mesh.eboTriangles) {
    wxLogError(
        "Mesh VAO EBO invariant failed in %s: vao=%u expectedEbo=%u currentEbo=%u",
        label, mesh.vao, mesh.eboTriangles, static_cast<GLuint>(currentEbo));
  }
#endif
  glBindVertexArray(0);
#ifndef NDEBUG
  GLint currentVao = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVao);
  if (currentVao != 0) {
    wxLogError("Mesh VAO remained bound after %s: vao=%u currentVao=%u",
               label, mesh.vao, static_cast<GLuint>(currentVao));
  }
#endif
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// Logs VAO and EBO state for macOS highlight diagnostics.
void LogMeshVaoDiagnostic(const Mesh &mesh, const char *label) {
#if defined(__APPLE__) && !defined(NDEBUG)
  GLint currentVao = 0;
  GLint currentEbo = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVao);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentEbo);
  wxLogDebug(
      "Highlight VAO diagnostic %s: meshVao=%u eboTriangles=%u eboLines=%u currentVao=%u currentEbo=%u",
      label, mesh.vao, mesh.eboTriangles, mesh.eboLines,
      static_cast<GLuint>(currentVao), static_cast<GLuint>(currentEbo));
#else
  (void)mesh;
  (void)label;
#endif
}

// Returns the prior front-face mode after adapting it for mirrored model draws.
GLint ApplyMirroredFrontFace(bool mirrored) {
  GLint previousFrontFace = GL_CCW;
  glGetIntegerv(GL_FRONT_FACE, &previousFrontFace);
  if (mirrored) {
    glFrontFace(previousFrontFace == GL_CCW ? GL_CW : GL_CCW);
  }
  return previousFrontFace;
}

// Restores the front-face winding mode saved before a mirrored draw.
void RestoreFrontFace(GLint previousFrontFace) {
  glFrontFace(static_cast<GLenum>(previousFrontFace));
}

// Returns the supplied model matrix or reads the current OpenGL model-view
// matrix.
const float *ResolveModelMatrixForMirroring(const float *modelMatrix,
                                            float fallbackModelMatrix[16]) {
  if (modelMatrix != nullptr)
    return modelMatrix;
  glGetFloatv(GL_MODELVIEW_MATRIX, fallbackModelMatrix);
  return fallbackModelMatrix;
}

} // namespace

void SceneRenderer::DrawMeshWithOutline(
    const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
    bool groupHighlight, bool selected, float cx, float cy, float cz,
    bool wireframe, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)>
        &captureTransform,
    bool unlit, const float *modelMatrix, bool disableDepthBias) {
  (void)cx;
  (void)cy;
  (void)cz;
  const bool forceDisableTexture = mode == Viewer2DRenderMode::Wireframe ||
                                   mode == Viewer2DRenderMode::ByFixtureType ||
                                   mode == Viewer2DRenderMode::ByLayer ||
                                   mode == Viewer2DRenderMode::ByUniverse;
  const GLboolean texture2DWasEnabled = glIsEnabled(GL_TEXTURE_2D);
  if (forceDisableTexture && texture2DWasEnabled)
    glDisable(GL_TEXTURE_2D);
  const auto restoreTextureState = [&]() {
    if (forceDisableTexture && texture2DWasEnabled)
      glEnable(GL_TEXTURE_2D);
  };

  if (wireframe) {
    float lineWidth =
        GetLineRenderProfile(m_controller.IsInteracting(),
                             mode == Viewer2DRenderMode::Wireframe,
                             m_controller.UseAdaptiveLineProfile())
            .lineWidth;
    bool symbolCaptureRenderProfile =
        mode == Viewer2DRenderMode::ByFixtureType ||
        ConfigManager::Get().GetFloat(
            "viewer3d_symbol_capture_render_profile") >= 0.5f;
    if (m_controller.GetSymbolCaptureRenderProfileOverride().has_value()) {
      symbolCaptureRenderProfile =
          m_controller.GetSymbolCaptureRenderProfileOverride().value();
    }
    const bool drawOutline = !m_controller.SkipOutlinesForCurrentFrame() &&
                             m_controller.IsSelectionOutlineEnabled2D() &&
                             (highlight || groupHighlight || selected);
    const bool useWireframeModeColor = mode == Viewer2DRenderMode::Wireframe;
    const float strokeR = useWireframeModeColor ? r : 0.0f;
    const float strokeG = useWireframeModeColor ? g : 0.0f;
    const float strokeB = useWireframeModeColor ? b : 0.0f;
    CanvasStroke stroke;
    stroke.color = {strokeR, strokeG, strokeB, 1.0f};
    stroke.width = lineWidth;
    if (!m_controller.IsCaptureOnly()) {
      const GLboolean lineSmoothWasEnabled = glIsEnabled(GL_LINE_SMOOTH);
      const GLboolean multisampleWasEnabled = glIsEnabled(GL_MULTISAMPLE);
      if (symbolCaptureRenderProfile) {
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_MULTISAMPLE);
      }
      if (drawOutline) {
        float glowWidth = lineWidth + 3.0f;
        glLineWidth(glowWidth);
        if (highlight)
          m_controller.SetGLColor(kPrimaryHighlightR, kPrimaryHighlightG,
                                  kPrimaryHighlightB);
        else if (groupHighlight)
          m_controller.SetGLColor(kGroupHighlightR, kGroupHighlightG,
                                  kGroupHighlightB);
        else if (selected)
          m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
        DrawMeshWireframe(mesh, scale, captureTransform);
      }
      glLineWidth(symbolCaptureRenderProfile ? 2.0f : lineWidth);
      m_controller.SetGLColor(strokeR, strokeG, strokeB);
      DrawMeshWireframe(mesh, scale, captureTransform, &stroke);
      if (symbolCaptureRenderProfile) {
        glLineWidth(2.0f);
        DrawMeshWireframe(mesh, scale, captureTransform, &stroke);
      }
      glLineWidth(lineWidth);
      m_controller.SetGLColor(strokeR, strokeG, strokeB);
      if (lineSmoothWasEnabled)
        glEnable(GL_LINE_SMOOTH);
      else
        glDisable(GL_LINE_SMOOTH);
      if (multisampleWasEnabled)
        glEnable(GL_MULTISAMPLE);
      else
        glDisable(GL_MULTISAMPLE);
    }
    if (m_controller.IsCaptureOnly())
      DrawMeshWireframe(mesh, scale, captureTransform, &stroke);
    if (m_controller.GetCaptureCanvas() &&
        mode != Viewer2DRenderMode::Wireframe) {
      CanvasFill fill;
      fill.color = {r, g, b, 1.0f};
      for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];
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
        if (!disableDepthBias) {
          glEnable(GL_POLYGON_OFFSET_FILL);
          glPolygonOffset(-1.0f, -1.0f);
        }
        m_controller.SetGLColor(r, g, b);
        if (unlit)
          glDisable(GL_LIGHTING);
        DrawMesh(mesh, scale, modelMatrix);
        if (unlit)
          glEnable(GL_LIGHTING);
        if (!disableDepthBias)
          glDisable(GL_POLYGON_OFFSET_FILL);
      }
    }
    restoreTextureState();
    return;
  }

  if (!m_controller.IsCaptureOnly()) {
    if (m_controller.IsWhiteModelStyleEnabled()) {
      const GLboolean texture2DWhiteModelWasEnabled =
          glIsEnabled(GL_TEXTURE_2D);
      if (texture2DWhiteModelWasEnabled)
        glDisable(GL_TEXTURE_2D);
      const bool useColorFillInWhiteStyle =
          !m_controller.IsSketchRenderStyleEnabled() &&
          (mode == Viewer2DRenderMode::ByFixtureType ||
           mode == Viewer2DRenderMode::ByLayer ||
           mode == Viewer2DRenderMode::ByUniverse);
      const bool usePureWhiteFillInWhiteMode =
          !m_controller.IsSketchRenderStyleEnabled() &&
          m_controller.IsPureWhiteRenderStyleEnabled();
      // Keep 3D white-model aligned with the 2D viewer draw order:
      // stroke pass first, then polygon-offset fill pass.
      const LineRenderProfile lineProfile = GetLineRenderProfile(
          m_controller.IsInteracting(), mode == Viewer2DRenderMode::Wireframe,
          m_controller.UseAdaptiveLineProfile());
      const float lineWidth = lineProfile.lineWidth;
      auto setHighlightOrSelectionColor = [&]() {
        if (highlight)
          m_controller.SetGLColor(kPrimaryHighlightR, kPrimaryHighlightG,
                                  kPrimaryHighlightB);
        else if (groupHighlight)
          m_controller.SetGLColor(kGroupHighlightR, kGroupHighlightG,
                                  kGroupHighlightB);
        else if (selected)
          m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
        else
          m_controller.SetGLColor(0.0f, 0.0f, 0.0f);
      };
      const bool drawOutline = !m_controller.SkipOutlinesForCurrentFrame() &&
                               !m_controller.IsSketchBasePassActive() &&
                               m_controller.IsSelectionOutlineEnabled2D() &&
                               (highlight || groupHighlight || selected);
      const bool interactiveSketchMode =
          m_controller.IsSketchRenderStyleEnabled() &&
          m_controller.IsInteracting();
      const SketchInteractionWireframeMode interactionMode =
          interactiveSketchMode ? ReadSketchInteractionWireframeMode()
                                : SketchInteractionWireframeMode::FullQuality;
      bool drawBaseWireframe = true;
      if (m_controller.IsSketchRenderStyleEnabled() &&
          (highlight || groupHighlight || selected)) {
        // The post-composite overlay supplies the colored fill and outline;
        // avoid repainting black Sketch ink over that visual feedback.
        drawBaseWireframe = false;
      }
      int wireframeTriangleStep = 1;
      if (interactiveSketchMode) {
        if (interactionMode == SketchInteractionWireframeMode::Sparse) {
          wireframeTriangleStep = ReadSketchInteractionWireframeStep();
        } else if (interactionMode ==
                   SketchInteractionWireframeMode::FillOnly) {
          drawBaseWireframe = false;
        } else if (interactionMode ==
                   SketchInteractionWireframeMode::HighlightSelectedOnly) {
          drawBaseWireframe =
              drawBaseWireframe && (highlight || groupHighlight || selected);
        }
      }

      if (!m_controller.IsCaptureOnly()) {
        if (drawOutline) {
          const float glowWidth = lineWidth + 3.0f;
          glLineWidth(glowWidth);
          setHighlightOrSelectionColor();
          DrawMeshWireframe(mesh, scale, captureTransform);
        }
        if (drawBaseWireframe) {
          glLineWidth(lineWidth);
          if (m_controller.IsSketchBasePassActive())
            glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
          else
            m_controller.SetGLColor(0.0f, 0.0f, 0.0f);
        }
      }
      if (drawBaseWireframe) {
        DrawMeshWireframe(mesh, scale, captureTransform, nullptr,
                          wireframeTriangleStep);
      }
      glLineWidth(1.0f);

      if (!disableDepthBias) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
      }
      if (m_controller.IsSketchBasePassActive()) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        DrawMesh(mesh, scale, modelMatrix);
      } else if (highlight || groupHighlight || selected) {
        setHighlightOrSelectionColor();
        const GLboolean highlightLightingWasEnabled = glIsEnabled(GL_LIGHTING);
        if (!highlightLightingWasEnabled)
          glEnable(GL_LIGHTING);
        LogMeshVaoDiagnostic(mesh, "before-highlight-draw");
        DrawMesh(mesh, scale, modelMatrix);
        LogMeshVaoDiagnostic(mesh, "after-highlight-draw");
        if (!highlightLightingWasEnabled)
          glDisable(GL_LIGHTING);
      } else if (usePureWhiteFillInWhiteMode) {
        const GLboolean fillLightingWasEnabled = glIsEnabled(GL_LIGHTING);
        if (fillLightingWasEnabled)
          glDisable(GL_LIGHTING);
        m_controller.SetGLColor(1.0f, 1.0f, 1.0f);
        DrawMesh(mesh, scale, modelMatrix);
        if (fillLightingWasEnabled)
          glEnable(GL_LIGHTING);
      } else if (useColorFillInWhiteStyle) {
        const GLboolean fillLightingWasEnabled = glIsEnabled(GL_LIGHTING);
        if (fillLightingWasEnabled)
          glDisable(GL_LIGHTING);
        m_controller.SetGLColor(r, g, b);
        DrawMesh(mesh, scale, modelMatrix);
        if (fillLightingWasEnabled)
          glEnable(GL_LIGHTING);
      } else {
        m_controller.SetGLColor(1.0f, 1.0f, 1.0f);
        DrawMesh(mesh, scale, modelMatrix);
      }
      if (!disableDepthBias)
        glDisable(GL_POLYGON_OFFSET_FILL);
      if (texture2DWhiteModelWasEnabled)
        glEnable(GL_TEXTURE_2D);
    } else {
      const bool useTexture =
          m_controller.IsTexturedRenderStyleEnabled() && !highlight &&
          !groupHighlight && !selected && mesh.textureId != 0 &&
          mesh.texcoords.size() >= (mesh.vertices.size() / 3u) * 2u;
      const bool useMaterialColor =
          m_controller.IsTexturedRenderStyleEnabled() && !highlight &&
          !groupHighlight && !selected && !useTexture &&
          mesh.hasMaterialBaseColor;
      if (highlight)
        m_controller.SetGLColor(kPrimaryHighlightR, kPrimaryHighlightG,
                                kPrimaryHighlightB);
      else if (groupHighlight)
        m_controller.SetGLColor(kGroupHighlightR, kGroupHighlightG,
                                kGroupHighlightB);
      else if (selected)
        m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
      else if (useMaterialColor)
        m_controller.SetGLColor(mesh.materialBaseColor[0],
                                mesh.materialBaseColor[1],
                                mesh.materialBaseColor[2]);
      else
        m_controller.SetGLColor(useTexture ? 1.0f : r, useTexture ? 1.0f : g,
                                useTexture ? 1.0f : b);

      if (unlit)
        glDisable(GL_LIGHTING);
      if (highlight || groupHighlight || selected)
        LogMeshVaoDiagnostic(mesh, "before-highlight-draw");
      DrawMesh(mesh, scale, modelMatrix, useTexture);
      if (highlight || groupHighlight || selected)
        LogMeshVaoDiagnostic(mesh, "after-highlight-draw");
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
      uint32_t i0 = mesh.indices[i];
      uint32_t i1 = mesh.indices[i + 1];
      uint32_t i2 = mesh.indices[i + 2];
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
  restoreTextureState();
}

// Draws mesh edges as wireframe lines and records them for capture output.
void SceneRenderer::DrawMeshWireframe(
    const Mesh &mesh, float scale,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)>
        &captureTransform,
    const CanvasStroke *captureStroke, int triangleStep) {
  const size_t triangleAdvance =
      static_cast<size_t>(std::max(1, triangleStep)) * 3u;
  const bool gpuHandlesValid = glIsBuffer(mesh.vboVertices) == GL_TRUE &&
                               glIsBuffer(mesh.eboLines) == GL_TRUE &&
                               glIsBuffer(mesh.eboTriangles) == GL_TRUE;
  const bool canUseGpuWireframe = mesh.buffersReady && mesh.vao != 0 &&
                                  mesh.vboVertices != 0 && mesh.eboLines != 0 &&
                                  mesh.eboTriangles != 0 && gpuHandlesValid &&
                                  triangleAdvance == 3u;

  if (!m_controller.IsCaptureOnly() && canUseGpuWireframe) {
    glBindVertexArray(mesh.vao);
    glPushMatrix();
    glScalef(scale, scale, scale);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vboVertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboLines);
    glDrawElements(GL_LINES, mesh.lineIndexCount, GL_UNSIGNED_INT, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);

    glDisableClientState(GL_VERTEX_ARRAY);
    RestoreMeshVaoElementBindingAndUnbind(mesh, "DrawMeshWireframe");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
  } else if (!m_controller.IsCaptureOnly()) {
    glBegin(GL_LINES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += triangleAdvance) {
      const uint32_t i0 = mesh.indices[i];
      const uint32_t i1 = mesh.indices[i + 1];
      const uint32_t i2 = mesh.indices[i + 2];

      glVertex3f(mesh.vertices[i0 * 3] * scale,
                 mesh.vertices[i0 * 3 + 1] * scale,
                 mesh.vertices[i0 * 3 + 2] * scale);
      glVertex3f(mesh.vertices[i1 * 3] * scale,
                 mesh.vertices[i1 * 3 + 1] * scale,
                 mesh.vertices[i1 * 3 + 2] * scale);

      glVertex3f(mesh.vertices[i1 * 3] * scale,
                 mesh.vertices[i1 * 3 + 1] * scale,
                 mesh.vertices[i1 * 3 + 2] * scale);
      glVertex3f(mesh.vertices[i2 * 3] * scale,
                 mesh.vertices[i2 * 3 + 1] * scale,
                 mesh.vertices[i2 * 3 + 2] * scale);

      glVertex3f(mesh.vertices[i2 * 3] * scale,
                 mesh.vertices[i2 * 3 + 1] * scale,
                 mesh.vertices[i2 * 3 + 2] * scale);
      glVertex3f(mesh.vertices[i0 * 3] * scale,
                 mesh.vertices[i0 * 3 + 1] * scale,
                 mesh.vertices[i0 * 3 + 2] * scale);
    }
    glEnd();
  }
  if (m_controller.GetCaptureCanvas()) {
    CanvasStroke stroke = captureStroke ? *captureStroke : CanvasStroke{};
    if (!captureStroke) {
      stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
      stroke.width = 1.0f;
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += triangleAdvance) {
      uint32_t i0 = mesh.indices[i];
      uint32_t i1 = mesh.indices[i + 1];
      uint32_t i2 = mesh.indices[i + 2];

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

// Draws a lit or textured mesh while preserving front-face orientation for
// mirrored transforms.
void SceneRenderer::DrawMesh(const Mesh &mesh, float scale,
                             const float *modelMatrix, bool useTexture) {
  const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
  if (cullWasEnabled)
    glDisable(GL_CULL_FACE);

  float fallbackModelMatrix[16];
  const float *effectiveModelMatrix =
      ResolveModelMatrixForMirroring(modelMatrix, fallbackModelMatrix);
  const bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  const bool mirrored = TransformDeterminant(effectiveModelMatrix) < 0.0f;
  const GLint previousFrontFace = ApplyMirroredFrontFace(mirrored);

  const std::vector<float> *normalData = &mesh.normals;
  const std::vector<uint32_t> *triangleIndices = &mesh.indices;

  const bool gpuHandlesValid = glIsBuffer(mesh.vboVertices) == GL_TRUE &&
                               glIsBuffer(mesh.vboNormals) == GL_TRUE &&
                               glIsBuffer(mesh.eboTriangles) == GL_TRUE;
  const bool flatGpuHandlesValid =
      glIsBuffer(mesh.vboFlatVertices) == GL_TRUE &&
      glIsBuffer(mesh.vboFlatNormals) == GL_TRUE;
  const bool canUseGpuTriangles =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboVertices != 0 &&
      mesh.vboNormals != 0 && mesh.eboTriangles != 0 && gpuHandlesValid &&
      mesh.triangleIndexCount > 0;

  GLint shadeModel = GL_SMOOTH;
  glGetIntegerv(GL_SHADE_MODEL, &shadeModel);
  const bool useFaceNormals = (shadeModel == GL_FLAT);

  const bool canUseGpuFlatTriangles =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboFlatVertices != 0 &&
      mesh.vboFlatNormals != 0 && mesh.flatVertexCount > 0 &&
      flatGpuHandlesValid;

  const bool allowGpuTriangles = canUseGpuTriangles && !useTexture &&
                                 (!useFaceNormals || !canUseGpuFlatTriangles);
  const bool allowGpuFlatTriangles = canUseGpuFlatTriangles && useFaceNormals;

  if (!m_controller.IsCaptureOnly() &&
      (allowGpuTriangles || allowGpuFlatTriangles)) {
    const bool textureEnabled =
        allowGpuTriangles && useTexture && mesh.textureId != 0 &&
        mesh.vboTexCoords != 0 &&
        mesh.texcoords.size() >= (mesh.vertices.size() / 3u) * 2u;
    glBindVertexArray(mesh.vao);
    glPushMatrix();
    glScalef(scale, scale, scale);

    glBindBuffer(GL_ARRAY_BUFFER, allowGpuFlatTriangles ? mesh.vboFlatVertices
                                                        : mesh.vboVertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER,
                 allowGpuFlatTriangles ? mesh.vboFlatNormals : mesh.vboNormals);
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, nullptr);

    if (textureEnabled) {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, mesh.textureId);
      glBindBuffer(GL_ARRAY_BUFFER, mesh.vboTexCoords);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
    }

    if (allowGpuFlatTriangles) {
      glDrawArrays(GL_TRIANGLES, 0, mesh.flatVertexCount);
    } else {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);
      glDrawElements(GL_TRIANGLES, mesh.triangleIndexCount, GL_UNSIGNED_INT,
                     nullptr);
    }

    if (textureEnabled) {
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      glBindTexture(GL_TEXTURE_2D, 0);
      glDisable(GL_TEXTURE_2D);
    }
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    RestoreMeshVaoElementBindingAndUnbind(mesh, "DrawMesh");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
  } else if (!m_controller.IsCaptureOnly()) {
    const bool textureEnabled =
        useTexture && mesh.textureId != 0 &&
        mesh.texcoords.size() >= (mesh.vertices.size() / 3u) * 2u;
    GLint twoSidedLightingWasEnabled = GL_FALSE;
    if (textureEnabled) {
      glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &twoSidedLightingWasEnabled);
      glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, mesh.textureId);
    }
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i + 2 < triangleIndices->size(); i += 3) {
      const uint32_t i0 = (*triangleIndices)[i];
      const uint32_t i1 = (*triangleIndices)[i + 1];
      const uint32_t i2 = (*triangleIndices)[i + 2];

      const float v0x = mesh.vertices[i0 * 3] * scale;
      const float v0y = mesh.vertices[i0 * 3 + 1] * scale;
      const float v0z = mesh.vertices[i0 * 3 + 2] * scale;
      const float v1x = mesh.vertices[i1 * 3] * scale;
      const float v1y = mesh.vertices[i1 * 3 + 1] * scale;
      const float v1z = mesh.vertices[i1 * 3 + 2] * scale;
      const float v2x = mesh.vertices[i2 * 3] * scale;
      const float v2y = mesh.vertices[i2 * 3 + 1] * scale;
      const float v2z = mesh.vertices[i2 * 3 + 2] * scale;

      if (useFaceNormals) {
        float nx = (v1y - v0y) * (v2z - v0z) - (v1z - v0z) * (v2y - v0y);
        float ny = (v1z - v0z) * (v2x - v0x) - (v1x - v0x) * (v2z - v0z);
        float nz = (v1x - v0x) * (v2y - v0y) - (v1y - v0y) * (v2x - v0x);
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) {
          nx /= len;
          ny /= len;
          nz /= len;
          if (hasNormals) {
            const float avgNx = ((*normalData)[i0 * 3] + (*normalData)[i1 * 3] +
                                 (*normalData)[i2 * 3]) /
                                3.0f;
            const float avgNy =
                ((*normalData)[i0 * 3 + 1] + (*normalData)[i1 * 3 + 1] +
                 (*normalData)[i2 * 3 + 1]) /
                3.0f;
            const float avgNz =
                ((*normalData)[i0 * 3 + 2] + (*normalData)[i1 * 3 + 2] +
                 (*normalData)[i2 * 3 + 2]) /
                3.0f;
            const float alignment = nx * avgNx + ny * avgNy + nz * avgNz;
            if (alignment < 0.0f) {
              nx = -nx;
              ny = -ny;
              nz = -nz;
            }
          }
          glNormal3f(nx, ny, nz);
        } else {
          glNormal3f(0.0f, 0.0f, 1.0f);
        }

        glVertex3f(v0x, v0y, v0z);
        glVertex3f(v1x, v1y, v1z);
        glVertex3f(v2x, v2y, v2z);
        continue;
      }

      if (hasNormals) {
        if (textureEnabled) {
          float faceNx = (v1y - v0y) * (v2z - v0z) - (v1z - v0z) * (v2y - v0y);
          float faceNy = (v1z - v0z) * (v2x - v0x) - (v1x - v0x) * (v2z - v0z);
          float faceNz = (v1x - v0x) * (v2y - v0y) - (v1y - v0y) * (v2x - v0x);
          const float faceLen =
              std::sqrt(faceNx * faceNx + faceNy * faceNy + faceNz * faceNz);
          if (faceLen > 0.0f) {
            faceNx /= faceLen;
            faceNy /= faceLen;
            faceNz /= faceLen;
            const float avgNx = ((*normalData)[i0 * 3] + (*normalData)[i1 * 3] +
                                 (*normalData)[i2 * 3]) /
                                3.0f;
            const float avgNy =
                ((*normalData)[i0 * 3 + 1] + (*normalData)[i1 * 3 + 1] +
                 (*normalData)[i2 * 3 + 1]) /
                3.0f;
            const float avgNz =
                ((*normalData)[i0 * 3 + 2] + (*normalData)[i1 * 3 + 2] +
                 (*normalData)[i2 * 3 + 2]) /
                3.0f;
            const float alignment =
                faceNx * avgNx + faceNy * avgNy + faceNz * avgNz;
            if (alignment < 0.0f) {
              faceNx = -faceNx;
              faceNy = -faceNy;
              faceNz = -faceNz;
            }
          } else {
            faceNx = 0.0f;
            faceNy = 0.0f;
            faceNz = 1.0f;
          }

          glNormal3f(faceNx, faceNy, faceNz);
          glTexCoord2f(mesh.texcoords[i0 * 2], mesh.texcoords[i0 * 2 + 1]);
          glVertex3f(v0x, v0y, v0z);
          glTexCoord2f(mesh.texcoords[i1 * 2], mesh.texcoords[i1 * 2 + 1]);
          glVertex3f(v1x, v1y, v1z);
          glTexCoord2f(mesh.texcoords[i2 * 2], mesh.texcoords[i2 * 2 + 1]);
          glVertex3f(v2x, v2y, v2z);
          continue;
        }
        glNormal3f((*normalData)[i0 * 3], (*normalData)[i0 * 3 + 1],
                   (*normalData)[i0 * 3 + 2]);
        glVertex3f(v0x, v0y, v0z);
        glNormal3f((*normalData)[i1 * 3], (*normalData)[i1 * 3 + 1],
                   (*normalData)[i1 * 3 + 2]);
        glVertex3f(v1x, v1y, v1z);
        glNormal3f((*normalData)[i2 * 3], (*normalData)[i2 * 3 + 1],
                   (*normalData)[i2 * 3 + 2]);
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
        if (textureEnabled) {
          glTexCoord2f(mesh.texcoords[i0 * 2], mesh.texcoords[i0 * 2 + 1]);
        }
        glVertex3f(v0x, v0y, v0z);
        if (textureEnabled) {
          glTexCoord2f(mesh.texcoords[i1 * 2], mesh.texcoords[i1 * 2 + 1]);
        }
        glVertex3f(v1x, v1y, v1z);
        if (textureEnabled) {
          glTexCoord2f(mesh.texcoords[i2 * 2], mesh.texcoords[i2 * 2 + 1]);
        }
        glVertex3f(v2x, v2y, v2z);
      }
    }
    glEnd();
    if (textureEnabled) {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDisable(GL_TEXTURE_2D);
      glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, twoSidedLightingWasEnabled);
    }
  }

  RestoreFrontFace(previousFrontFace);
  if (cullWasEnabled)
    glEnable(GL_CULL_FACE);
}

// Draw the 2D reference grid with configurable spacing and a moderate centered
// extent.
void SceneRenderer::DrawGrid(int style, float r, float g, float b,
                             Viewer2DView view) {
  const float step =
      std::max(0.01f, ConfigManager::Get().GetFloat("grid_spacing_m"));
  const float halfCellCount = 40.0f;
  const float size = step * halfCellCount;

  const LineRenderProfile profile =
      GetLineRenderProfile(m_controller.IsInteracting(), true,
                           m_controller.UseAdaptiveLineProfile());
  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = profile.lineWidth;

  const GLboolean lineSmoothWasEnabled = glIsEnabled(GL_LINE_SMOOTH);
  const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
  if (lightingWasEnabled)
    glDisable(GL_LIGHTING);
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
        if (m_controller.GetCaptureCanvas() &&
            m_controller.CaptureIncludesGrid()) {
          m_controller.RecordLine({i, -size, 0.0f}, {i, size, 0.0f}, stroke);
          m_controller.RecordLine({-size, i, 0.0f}, {size, i, 0.0f}, stroke);
        }
        break;
      case Viewer2DView::Front:
        glVertex3f(i, 0.0f, -size);
        glVertex3f(i, 0.0f, size);
        glVertex3f(-size, 0.0f, i);
        glVertex3f(size, 0.0f, i);
        if (m_controller.GetCaptureCanvas() &&
            m_controller.CaptureIncludesGrid()) {
          m_controller.RecordLine({i, 0.0f, -size}, {i, 0.0f, size}, stroke);
          m_controller.RecordLine({-size, 0.0f, i}, {size, 0.0f, i}, stroke);
        }
        break;
      case Viewer2DView::Side:
        glVertex3f(0.0f, i, -size);
        glVertex3f(0.0f, i, size);
        glVertex3f(0.0f, -size, i);
        glVertex3f(0.0f, size, i);
        if (m_controller.GetCaptureCanvas() &&
            m_controller.CaptureIncludesGrid()) {
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
          if (m_controller.GetCaptureCanvas() &&
              m_controller.CaptureIncludesGrid())
            m_controller.RecordLine({x, y, 0.0f}, {x, y, 0.0f}, stroke);
          break;
        case Viewer2DView::Front:
          glVertex3f(x, 0.0f, y);
          if (m_controller.GetCaptureCanvas() &&
              m_controller.CaptureIncludesGrid())
            m_controller.RecordLine({x, 0.0f, y}, {x, 0.0f, y}, stroke);
          break;
        case Viewer2DView::Side:
          glVertex3f(0.0f, x, y);
          if (m_controller.GetCaptureCanvas() &&
              m_controller.CaptureIncludesGrid())
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
          if (m_controller.GetCaptureCanvas() &&
              m_controller.CaptureIncludesGrid()) {
            m_controller.RecordLine({x - half, y, 0.0f}, {x + half, y, 0.0f},
                                    stroke);
            m_controller.RecordLine({x, y - half, 0.0f}, {x, y + half, 0.0f},
                                    stroke);
          }
          break;
        case Viewer2DView::Front:
          glVertex3f(x - half, 0.0f, y);
          glVertex3f(x + half, 0.0f, y);
          glVertex3f(x, 0.0f, y - half);
          glVertex3f(x, 0.0f, y + half);
          if (m_controller.GetCaptureCanvas() &&
              m_controller.CaptureIncludesGrid()) {
            m_controller.RecordLine({x - half, 0.0f, y}, {x + half, 0.0f, y},
                                    stroke);
            m_controller.RecordLine({x, 0.0f, y - half}, {x, 0.0f, y + half},
                                    stroke);
          }
          break;
        case Viewer2DView::Side:
          glVertex3f(0.0f, x - half, y);
          glVertex3f(0.0f, x + half, y);
          glVertex3f(0.0f, x, y - half);
          glVertex3f(0.0f, x, y + half);
          if (m_controller.GetCaptureCanvas() &&
              m_controller.CaptureIncludesGrid()) {
            m_controller.RecordLine({0.0f, x - half, y}, {0.0f, x + half, y},
                                    stroke);
            m_controller.RecordLine({0.0f, x, y - half}, {0.0f, x, y + half},
                                    stroke);
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

  if (lightingWasEnabled)
    glEnable(GL_LIGHTING);
}

void SceneRenderer::SetupMaterialFromRGB(float r, float g, float b) {
  m_controller.SetGLColor(r, g, b);
}
