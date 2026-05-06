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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "configmanager.h"

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

enum class SketchInteractionWireframeMode {
  FullQuality = 0,
  Sparse = 1,
  FillOnly = 2,
  HighlightSelectedOnly = 3
};

SketchInteractionWireframeMode ReadSketchInteractionWireframeMode() {
  const float rawValue =
      ConfigManager::Get().GetFloat("viewer3d_sketch_interaction_wireframe_mode");
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

int ReadSketchInteractionWireframeStep() {
  const float rawStep =
      ConfigManager::Get().GetFloat("viewer3d_sketch_interaction_wireframe_step");
  const int step = static_cast<int>(std::lround(rawStep));
  return std::clamp(step, 1, 12);
}

LineRenderProfile GetLineRenderProfile(bool isInteracting, bool wireframeMode,
                                       bool adaptiveEnabled) {
  (void)isInteracting;
  if (!adaptiveEnabled)
    return {wireframeMode ? 1.0f : 2.0f, false};
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
  // Match OpenGL fixed-function normal transform (inverse-transpose of the
  // model's 3x3 linear part) used by the standard/textured lighting paths.
  const float m00 = modelMatrix[0];
  const float m01 = modelMatrix[4];
  const float m02 = modelMatrix[8];
  const float m10 = modelMatrix[1];
  const float m11 = modelMatrix[5];
  const float m12 = modelMatrix[9];
  const float m20 = modelMatrix[2];
  const float m21 = modelMatrix[6];
  const float m22 = modelMatrix[10];

  const float c00 = m11 * m22 - m12 * m21;
  const float c01 = m12 * m20 - m10 * m22;
  const float c02 = m10 * m21 - m11 * m20;
  const float c10 = m02 * m21 - m01 * m22;
  const float c11 = m00 * m22 - m02 * m20;
  const float c12 = m01 * m20 - m00 * m21;
  const float c20 = m01 * m12 - m02 * m11;
  const float c21 = m02 * m10 - m00 * m12;
  const float c22 = m00 * m11 - m01 * m10;
  const float det = m00 * c00 + m01 * c01 + m02 * c02;
  if (std::fabs(det) <= 1e-8f)
    return NormalizeVector(n[0], n[1], n[2]);
  const float invDet = 1.0f / det;

  const float x = (c00 * n[0] + c10 * n[1] + c20 * n[2]) * invDet;
  const float y = (c01 * n[0] + c11 * n[1] + c21 * n[2]) * invDet;
  const float z = (c02 * n[0] + c12 * n[1] + c22 * n[2]) * invDet;
  return NormalizeVector(x, y, z);
}

std::array<float, 3> TransformPoint(const std::array<float, 3> &p,
                                    const float *modelMatrix) {
  if (!modelMatrix)
    return p;
  const float x = modelMatrix[0] * p[0] + modelMatrix[4] * p[1] +
                  modelMatrix[8] * p[2] + modelMatrix[12];
  const float y = modelMatrix[1] * p[0] + modelMatrix[5] * p[1] +
                  modelMatrix[9] * p[2] + modelMatrix[13];
  const float z = modelMatrix[2] * p[0] + modelMatrix[6] * p[1] +
                  modelMatrix[10] * p[2] + modelMatrix[14];
  return {x, y, z};
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

struct ThreeToneInkProgram {
  GLuint program = 0;
  GLint positionAttrib = -1;
  GLint normalAttrib = -1;
  GLint modelViewUniform = -1;
  GLint projectionUniform = -1;
  GLint normalMatrixUniform = -1;
  GLint lightDirUniform = -1;
  GLint darkToneUniform = -1;
  GLint midToneUniform = -1;
  GLint lightToneUniform = -1;
  GLint darkThresholdUniform = -1;
  GLint lightThresholdUniform = -1;
};

GLuint CompileShader(GLenum shaderType, const char *source) {
  const GLuint shader = glCreateShader(shaderType);
  if (shader == 0)
    return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE)
    return shader;
  glDeleteShader(shader);
  return 0;
}

bool LinkProgram(GLuint program) {
  glLinkProgram(program);
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  return linked == GL_TRUE;
}

ThreeToneInkProgram CreateThreeToneInkProgram() {
  static constexpr const char *kVertexShader = R"glsl(
    #version 120
    attribute vec3 aPosition;
    attribute vec3 aNormal;
    uniform mat4 uModelView;
    uniform mat4 uProjection;
    uniform mat3 uNormalMatrix;
    varying vec3 vNormal;
    void main() {
      vNormal = normalize(uNormalMatrix * aNormal);
      gl_Position = uProjection * uModelView * vec4(aPosition, 1.0);
    }
  )glsl";
  static constexpr const char *kFragmentShader = R"glsl(
    #version 120
    uniform vec3 uLightDir;
    uniform vec3 uDarkTone;
    uniform vec3 uMidTone;
    uniform vec3 uLightTone;
    uniform float uDarkThreshold;
    uniform float uLightThreshold;
    varying vec3 vNormal;
    void main() {
      float ndotl = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
      vec3 tone = uLightTone;
      if (ndotl <= uDarkThreshold)
        tone = uDarkTone;
      else if (ndotl <= uLightThreshold)
        tone = uMidTone;
      gl_FragColor = vec4(tone, 1.0);
    }
  )glsl";

  ThreeToneInkProgram result;
  const GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
  if (vs == 0 || fs == 0) {
    if (vs != 0)
      glDeleteShader(vs);
    if (fs != 0)
      glDeleteShader(fs);
    return result;
  }

  result.program = glCreateProgram();
  if (result.program == 0) {
    glDeleteShader(vs);
    glDeleteShader(fs);
    return result;
  }
  glAttachShader(result.program, vs);
  glAttachShader(result.program, fs);
  glBindAttribLocation(result.program, 0, "aPosition");
  glBindAttribLocation(result.program, 1, "aNormal");
  const bool linked = LinkProgram(result.program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!linked) {
    glDeleteProgram(result.program);
    result.program = 0;
    return result;
  }

  result.positionAttrib = glGetAttribLocation(result.program, "aPosition");
  result.normalAttrib = glGetAttribLocation(result.program, "aNormal");
  result.modelViewUniform = glGetUniformLocation(result.program, "uModelView");
  result.projectionUniform = glGetUniformLocation(result.program, "uProjection");
  result.normalMatrixUniform =
      glGetUniformLocation(result.program, "uNormalMatrix");
  result.lightDirUniform = glGetUniformLocation(result.program, "uLightDir");
  result.darkToneUniform = glGetUniformLocation(result.program, "uDarkTone");
  result.midToneUniform = glGetUniformLocation(result.program, "uMidTone");
  result.lightToneUniform = glGetUniformLocation(result.program, "uLightTone");
  result.darkThresholdUniform =
      glGetUniformLocation(result.program, "uDarkThreshold");
  result.lightThresholdUniform =
      glGetUniformLocation(result.program, "uLightThreshold");
  return result;
}

const ThreeToneInkProgram &GetThreeToneInkProgram() {
  static const ThreeToneInkProgram program = CreateThreeToneInkProgram();
  return program;
}

void ComputeNormalMatrix3x3(const float *modelView, float *normalMatrix3x3) {
  const float m00 = modelView[0];
  const float m01 = modelView[4];
  const float m02 = modelView[8];
  const float m10 = modelView[1];
  const float m11 = modelView[5];
  const float m12 = modelView[9];
  const float m20 = modelView[2];
  const float m21 = modelView[6];
  const float m22 = modelView[10];

  const float c00 = m11 * m22 - m12 * m21;
  const float c01 = m12 * m20 - m10 * m22;
  const float c02 = m10 * m21 - m11 * m20;
  const float c10 = m02 * m21 - m01 * m22;
  const float c11 = m00 * m22 - m02 * m20;
  const float c12 = m01 * m20 - m00 * m21;
  const float c20 = m01 * m12 - m02 * m11;
  const float c21 = m02 * m10 - m00 * m12;
  const float c22 = m00 * m11 - m01 * m10;
  const float det = m00 * c00 + m01 * c01 + m02 * c02;
  if (std::fabs(det) <= 1e-8f) {
    normalMatrix3x3[0] = 1.0f;
    normalMatrix3x3[1] = 0.0f;
    normalMatrix3x3[2] = 0.0f;
    normalMatrix3x3[3] = 0.0f;
    normalMatrix3x3[4] = 1.0f;
    normalMatrix3x3[5] = 0.0f;
    normalMatrix3x3[6] = 0.0f;
    normalMatrix3x3[7] = 0.0f;
    normalMatrix3x3[8] = 1.0f;
    return;
  }
  const float invDet = 1.0f / det;
  normalMatrix3x3[0] = c00 * invDet;
  normalMatrix3x3[1] = c10 * invDet;
  normalMatrix3x3[2] = c20 * invDet;
  normalMatrix3x3[3] = c01 * invDet;
  normalMatrix3x3[4] = c11 * invDet;
  normalMatrix3x3[5] = c21 * invDet;
  normalMatrix3x3[6] = c02 * invDet;
  normalMatrix3x3[7] = c12 * invDet;
  normalMatrix3x3[8] = c22 * invDet;
}

bool DrawMeshThreeToneInkGpu(const Mesh &mesh, float scale) {
  const bool gpuHandlesValid = glIsBuffer(mesh.vboVertices) == GL_TRUE &&
                               glIsBuffer(mesh.vboNormals) == GL_TRUE &&
                               glIsBuffer(mesh.eboTriangles) == GL_TRUE;
  const bool canUseGpuPath =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboVertices != 0 &&
      mesh.vboNormals != 0 && mesh.eboTriangles != 0 && gpuHandlesValid &&
      mesh.triangleIndexCount > 0;
  if (!canUseGpuPath)
    return false;

  const ThreeToneInkProgram &program = GetThreeToneInkProgram();
  if (program.program == 0 || program.positionAttrib < 0 ||
      program.normalAttrib < 0 || program.modelViewUniform < 0 ||
      program.projectionUniform < 0 || program.normalMatrixUniform < 0 ||
      program.lightDirUniform < 0 || program.darkToneUniform < 0 ||
      program.midToneUniform < 0 || program.lightToneUniform < 0 ||
      program.darkThresholdUniform < 0 || program.lightThresholdUniform < 0) {
    return false;
  }

  GLint priorProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &priorProgram);

  glBindVertexArray(mesh.vao);
  glPushMatrix();
  glScalef(scale, scale, scale);

  float modelView[16];
  float projection[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
  glGetFloatv(GL_PROJECTION_MATRIX, projection);

  float normalMatrix[9];
  ComputeNormalMatrix3x3(modelView, normalMatrix);

  glUseProgram(program.program);
  glUniformMatrix4fv(program.modelViewUniform, 1, GL_FALSE, modelView);
  glUniformMatrix4fv(program.projectionUniform, 1, GL_FALSE, projection);
  glUniformMatrix3fv(program.normalMatrixUniform, 1, GL_FALSE, normalMatrix);
  const std::array<float, 3> lightDir = NormalizeVector(0.35f, -0.55f, 1.0f);
  glUniform3f(program.lightDirUniform, lightDir[0], lightDir[1], lightDir[2]);
  glUniform3f(program.darkToneUniform, 0.62f, 0.62f, 0.62f);
  glUniform3f(program.midToneUniform, 0.84f, 0.84f, 0.84f);
  glUniform3f(program.lightToneUniform, 1.0f, 1.0f, 1.0f);
  glUniform1f(program.darkThresholdUniform, 0.10f);
  glUniform1f(program.lightThresholdUniform, 0.30f);

  glBindBuffer(GL_ARRAY_BUFFER, mesh.vboVertices);
  glEnableVertexAttribArray(static_cast<GLuint>(program.positionAttrib));
  glVertexAttribPointer(static_cast<GLuint>(program.positionAttrib), 3, GL_FLOAT,
                        GL_FALSE, 0, nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
  glEnableVertexAttribArray(static_cast<GLuint>(program.normalAttrib));
  glVertexAttribPointer(static_cast<GLuint>(program.normalAttrib), 3, GL_FLOAT,
                        GL_FALSE, 0, nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);
  glDrawElements(GL_TRIANGLES, mesh.triangleIndexCount, GL_UNSIGNED_INT,
                 nullptr);

  glDisableVertexAttribArray(static_cast<GLuint>(program.normalAttrib));
  glDisableVertexAttribArray(static_cast<GLuint>(program.positionAttrib));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(static_cast<GLuint>(priorProgram));
  glPopMatrix();

  return true;
}

void DrawMeshThreeToneInkImmediate(const Mesh &mesh, float scale,
                                   const float *modelMatrix);

void EnsureFlippedFlatCache(const Mesh &mesh) {
  if (!mesh.flippedFlatVertices.empty() && !mesh.flippedFlatNormals.empty())
    return;

  mesh.flippedFlatVertices.clear();
  mesh.flippedFlatNormals.clear();
  if (mesh.vertices.empty() || mesh.indices.empty())
    return;

  mesh.flippedFlatVertices.reserve(mesh.indices.size() * 3);
  mesh.flippedFlatNormals.reserve(mesh.indices.size() * 3);

  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const uint32_t i0 = mesh.indices[i];
    const uint32_t i1 = mesh.indices[i + 2];
    const uint32_t i2 = mesh.indices[i + 1];

    const float v0x = mesh.vertices[i0 * 3];
    const float v0y = mesh.vertices[i0 * 3 + 1];
    const float v0z = mesh.vertices[i0 * 3 + 2];
    const float v1x = mesh.vertices[i1 * 3];
    const float v1y = mesh.vertices[i1 * 3 + 1];
    const float v1z = mesh.vertices[i1 * 3 + 2];
    const float v2x = mesh.vertices[i2 * 3];
    const float v2y = mesh.vertices[i2 * 3 + 1];
    const float v2z = mesh.vertices[i2 * 3 + 2];

    const std::array<float, 3> faceNormal = NormalizeVector(
        (v1y - v0y) * (v2z - v0z) - (v1z - v0z) * (v2y - v0y),
        (v1z - v0z) * (v2x - v0x) - (v1x - v0x) * (v2z - v0z),
        (v1x - v0x) * (v2y - v0y) - (v1y - v0y) * (v2x - v0x));

    mesh.flippedFlatVertices.insert(
        mesh.flippedFlatVertices.end(),
        {v0x, v0y, v0z, v1x, v1y, v1z, v2x, v2y, v2z});
    for (int v = 0; v < 3; ++v) {
      mesh.flippedFlatNormals.push_back(faceNormal[0]);
      mesh.flippedFlatNormals.push_back(faceNormal[1]);
      mesh.flippedFlatNormals.push_back(faceNormal[2]);
    }
  }
}

void DrawMeshThreeToneInk(const Mesh &mesh, float scale, const float *modelMatrix) {
  if (DrawMeshThreeToneInkGpu(mesh, scale))
    return;
  DrawMeshThreeToneInkImmediate(mesh, scale, modelMatrix);
}

void DrawMeshThreeToneInkImmediate(const Mesh &mesh, float scale,
                                   const float *modelMatrix) {
  std::array<float, 3> lightDir = NormalizeVector(0.35f, -0.55f, 1.0f);
  const bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  const bool flipWinding =
      (modelMatrix != nullptr) && TransformDeterminant(modelMatrix) < 0.0f;
  const std::vector<uint32_t> *triangleIndices = &mesh.indices;
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
    const uint32_t tri[3] = {(*triangleIndices)[i], (*triangleIndices)[i + 1],
                                   (*triangleIndices)[i + 2]};
    const float v0x = mesh.vertices[tri[0] * 3];
    const float v0y = mesh.vertices[tri[0] * 3 + 1];
    const float v0z = mesh.vertices[tri[0] * 3 + 2];
    const float v1x = mesh.vertices[tri[1] * 3];
    const float v1y = mesh.vertices[tri[1] * 3 + 1];
    const float v1z = mesh.vertices[tri[1] * 3 + 2];
    const float v2x = mesh.vertices[tri[2] * 3];
    const float v2y = mesh.vertices[tri[2] * 3 + 1];
    const float v2z = mesh.vertices[tri[2] * 3 + 2];

    const float ux = v1x - v0x;
    const float uy = v1y - v0y;
    const float uz = v1z - v0z;
    const float vx = v2x - v0x;
    const float vy = v2y - v0y;
    const float vz = v2z - v0z;
    const std::array<float, 3> triangleNormal = NormalizeVector(
        uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx);
    const std::array<float, 3> p0World =
        TransformPoint({v0x, v0y, v0z}, modelMatrix);
    const std::array<float, 3> p1World =
        TransformPoint({v1x, v1y, v1z}, modelMatrix);
    const std::array<float, 3> p2World =
        TransformPoint({v2x, v2y, v2z}, modelMatrix);
    const std::array<float, 3> worldTriNormal = NormalizeVector(
        (p1World[1] - p0World[1]) * (p2World[2] - p0World[2]) -
            (p1World[2] - p0World[2]) * (p2World[1] - p0World[1]),
        (p1World[2] - p0World[2]) * (p2World[0] - p0World[0]) -
            (p1World[0] - p0World[0]) * (p2World[2] - p0World[2]),
        (p1World[0] - p0World[0]) * (p2World[1] - p0World[1]) -
            (p1World[1] - p0World[1]) * (p2World[0] - p0World[0]));

    for (int v = 0; v < 3; ++v) {
      const uint32_t idx = tri[v];
      const float vx = mesh.vertices[idx * 3] * scale;
      const float vy = mesh.vertices[idx * 3 + 1] * scale;
      const float vz = mesh.vertices[idx * 3 + 2] * scale;

      std::array<float, 3> localNormal = triangleNormal;
      if (hasNormals) {
        const float nx = mesh.normals[idx * 3];
        const float ny = mesh.normals[idx * 3 + 1];
        const float nz = mesh.normals[idx * 3 + 2];
        const float normalLenSq = nx * nx + ny * ny + nz * nz;
        if (normalLenSq > 1e-12f) {
          localNormal = NormalizeVector(nx, ny, nz);
        }
      }

      std::array<float, 3> worldNormal = TransformNormal(localNormal, modelMatrix);
      const float dotWorld = worldNormal[0] * worldTriNormal[0] +
                             worldNormal[1] * worldTriNormal[1] +
                             worldNormal[2] * worldTriNormal[2];
      if (dotWorld < 0.0f) {
        worldNormal[0] = -worldNormal[0];
        worldNormal[1] = -worldNormal[1];
        worldNormal[2] = -worldNormal[2];
      }

      const float ndotl = worldNormal[0] * lightDir[0] +
                          worldNormal[1] * lightDir[1] +
                          worldNormal[2] * lightDir[2];
      const float diffuse = std::max(0.0f, ndotl);
      const InkColor tone = QuantizeInkTone(diffuse);
      glColor3f(tone.r, tone.g, tone.b);
      glNormal3f(worldNormal[0], worldNormal[1], worldNormal[2]);
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
    bool unlit, const float *modelMatrix, bool disableDepthBias) {
  (void)cx;
  (void)cy;
  (void)cz;
  const bool forceDisableTexture =
      mode == Viewer2DRenderMode::Wireframe ||
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
        ConfigManager::Get().GetFloat("viewer3d_symbol_capture_render_profile") >=
            0.5f;
    if (m_controller.GetSymbolCaptureRenderProfileOverride().has_value()) {
      symbolCaptureRenderProfile =
          m_controller.GetSymbolCaptureRenderProfileOverride().value();
    }
    const bool drawOutline =
        !m_controller.SkipOutlinesForCurrentFrame() &&
        m_controller.IsSelectionOutlineEnabled2D() && (highlight || selected);
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
          m_controller.SetGLColor(0.0f, 1.0f, 0.0f);
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
    if (m_controller.GetCaptureCanvas() && mode != Viewer2DRenderMode::Wireframe) {
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
      const GLboolean texture2DWhiteModelWasEnabled = glIsEnabled(GL_TEXTURE_2D);
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
      const LineRenderProfile lineProfile =
          GetLineRenderProfile(m_controller.IsInteracting(),
                               mode == Viewer2DRenderMode::Wireframe,
                               m_controller.UseAdaptiveLineProfile());
      const float lineWidth = lineProfile.lineWidth;
      auto setHighlightOrSelectionColor = [&]() {
        if (highlight)
          m_controller.SetGLColor(0.0f, 1.0f, 0.0f);
        else if (selected)
          m_controller.SetGLColor(0.0f, 1.0f, 1.0f);
        else
          m_controller.SetGLColor(0.0f, 0.0f, 0.0f);
      };
      const bool drawOutline =
          !m_controller.SkipOutlinesForCurrentFrame() &&
          m_controller.IsSelectionOutlineEnabled2D() && (highlight || selected);
      const bool interactiveSketchMode =
          m_controller.IsSketchRenderStyleEnabled() && m_controller.IsInteracting();
      const SketchInteractionWireframeMode interactionMode =
          interactiveSketchMode ? ReadSketchInteractionWireframeMode()
                                : SketchInteractionWireframeMode::FullQuality;
      bool drawBaseWireframe = true;
      int wireframeTriangleStep = 1;
      if (interactiveSketchMode) {
        if (interactionMode == SketchInteractionWireframeMode::Sparse) {
          wireframeTriangleStep = ReadSketchInteractionWireframeStep();
        } else if (interactionMode == SketchInteractionWireframeMode::FillOnly) {
          drawBaseWireframe = false;
        } else if (interactionMode ==
                   SketchInteractionWireframeMode::HighlightSelectedOnly) {
          drawBaseWireframe = highlight || selected;
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
      if (highlight || selected) {
        setHighlightOrSelectionColor();
        if (!glIsEnabled(GL_LIGHTING))
          glEnable(GL_LIGHTING);
        DrawMesh(mesh, scale, modelMatrix);
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
        const GLboolean fillLightingWasEnabled = glIsEnabled(GL_LIGHTING);
        if (fillLightingWasEnabled)
          glDisable(GL_LIGHTING);
        DrawMeshThreeToneInk(mesh, scale, modelMatrix);
        if (fillLightingWasEnabled)
          glEnable(GL_LIGHTING);
      }
      if (!disableDepthBias)
        glDisable(GL_POLYGON_OFFSET_FILL);
      if (texture2DWhiteModelWasEnabled)
        glEnable(GL_TEXTURE_2D);
    } else {
      const bool useTexture = m_controller.IsTexturedRenderStyleEnabled() &&
                              !highlight && !selected &&
                              mesh.textureId != 0 &&
                              mesh.texcoords.size() >= (mesh.vertices.size() / 3u) * 2u;
      const bool useMaterialColor =
                                    m_controller.IsTexturedRenderStyleEnabled() &&
                                    !highlight && !selected &&
                                    !useTexture && mesh.hasMaterialBaseColor;
      if (highlight)
        m_controller.SetGLColor(0.0f, 1.0f, 0.0f);
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
      DrawMesh(mesh, scale, modelMatrix, useTexture);
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

void SceneRenderer::DrawMeshWireframe(
    const Mesh &mesh, float scale,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform,
    const CanvasStroke *captureStroke, int triangleStep) {
  const size_t triangleAdvance =
      static_cast<size_t>(std::max(1, triangleStep)) * 3u;
  const bool gpuHandlesValid = glIsBuffer(mesh.vboVertices) == GL_TRUE &&
                               glIsBuffer(mesh.eboLines) == GL_TRUE &&
                               glIsBuffer(mesh.eboTriangles) == GL_TRUE;
  const bool canUseGpuWireframe =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboVertices != 0 &&
      mesh.eboLines != 0 && mesh.eboTriangles != 0 && gpuHandlesValid &&
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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
  } else if (!m_controller.IsCaptureOnly()) {
    glBegin(GL_LINES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += triangleAdvance) {
      const uint32_t i0 = mesh.indices[i];
      const uint32_t i1 = mesh.indices[i + 1];
      const uint32_t i2 = mesh.indices[i + 2];

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

void SceneRenderer::DrawMesh(const Mesh &mesh, float scale, const float *modelMatrix,
                             bool useTexture) {
  const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
  if (cullWasEnabled)
    glDisable(GL_CULL_FACE);

  const bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
  const bool flipWinding =
      (modelMatrix != nullptr) && TransformDeterminant(modelMatrix) < 0.0f;

  const std::vector<uint32_t> *triangleIndices = &mesh.indices;
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

  const bool canUseGpuVertexArrays =
      mesh.buffersReady && mesh.vao != 0 && mesh.vboVertices != 0 &&
      mesh.vboNormals != 0 && gpuHandlesValid;

  const bool hasFlippedFlatCache = [&]() {
    if (!flipWinding)
      return false;
    EnsureFlippedFlatCache(mesh);
    return !mesh.flippedFlatVertices.empty() && !mesh.flippedFlatNormals.empty();
  }();

  const bool allowGpuTriangles =
      canUseGpuTriangles && (!useFaceNormals || !canUseGpuFlatTriangles);
  const bool allowGpuFlatTriangles = canUseGpuFlatTriangles && useFaceNormals;
  const bool allowCachedFlatTriangles = useFaceNormals && hasFlippedFlatCache;
  const bool allowDrawElementsWithCpuIndices =
      canUseGpuVertexArrays && !useFaceNormals && flipWinding;

  if (!m_controller.IsCaptureOnly() &&
      (allowGpuTriangles || allowGpuFlatTriangles || allowCachedFlatTriangles ||
       allowDrawElementsWithCpuIndices)) {
    const bool textureEnabled =
        allowGpuTriangles && useTexture && mesh.textureId != 0 &&
        mesh.vboTexCoords != 0 &&
        mesh.texcoords.size() >= (mesh.vertices.size() / 3u) * 2u;
    glBindVertexArray(mesh.vao);
    glPushMatrix();
    glScalef(scale, scale, scale);

    glBindBuffer(GL_ARRAY_BUFFER,
                 allowGpuFlatTriangles ? mesh.vboFlatVertices : mesh.vboVertices);
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

    if (allowCachedFlatTriangles) {
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glVertexPointer(3, GL_FLOAT, 0, mesh.flippedFlatVertices.data());
      glNormalPointer(GL_FLOAT, 0, mesh.flippedFlatNormals.data());
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glDrawArrays(
          GL_TRIANGLES, 0,
          static_cast<GLsizei>(mesh.flippedFlatVertices.size() / 3));
    } else if (allowGpuFlatTriangles) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glDrawArrays(GL_TRIANGLES, 0, mesh.flatVertexCount);
    } else if (allowDrawElementsWithCpuIndices) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glDrawElements(
          GL_TRIANGLES, static_cast<GLsizei>(triangleIndices->size()),
          GL_UNSIGNED_INT, triangleIndices->data());
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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPopMatrix();
  } else if (!m_controller.IsCaptureOnly()) {
    const bool textureEnabled =
        useTexture && mesh.textureId != 0 &&
        mesh.texcoords.size() >= (mesh.vertices.size() / 3u) * 2u;
    if (textureEnabled) {
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

      const auto &normalData = mesh.normals;

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
            const float avgNx =
                (normalData[i0 * 3] + normalData[i1 * 3] + normalData[i2 * 3]) /
                3.0f;
            const float avgNy = (normalData[i0 * 3 + 1] + normalData[i1 * 3 + 1] +
                                 normalData[i2 * 3 + 1]) /
                                3.0f;
            const float avgNz = (normalData[i0 * 3 + 2] + normalData[i1 * 3 + 2] +
                                 normalData[i2 * 3 + 2]) /
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
          glTexCoord2f(mesh.texcoords[i0 * 2], mesh.texcoords[i0 * 2 + 1]);
        }
        glNormal3f(normalData[i0 * 3], normalData[i0 * 3 + 1],
                   normalData[i0 * 3 + 2]);
        glVertex3f(v0x, v0y, v0z);
        if (textureEnabled) {
          glTexCoord2f(mesh.texcoords[i1 * 2], mesh.texcoords[i1 * 2 + 1]);
        }
        glNormal3f(normalData[i1 * 3], normalData[i1 * 3 + 1],
                   normalData[i1 * 3 + 2]);
        glVertex3f(v1x, v1y, v1z);
        if (textureEnabled) {
          glTexCoord2f(mesh.texcoords[i2 * 2], mesh.texcoords[i2 * 2 + 1]);
        }
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
    }
  }

  if (cullWasEnabled)
    glEnable(GL_CULL_FACE);
}

// Draw the 2D reference grid using the configured world-space span and style.
void SceneRenderer::DrawGrid(int style, float r, float g, float b,
                             Viewer2DView view) {
  const float size = std::max(1.0f, ConfigManager::Get().GetFloat("grid_size_m"));
  const float step = 1.0f;

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

  if (lightingWasEnabled)
    glEnable(GL_LIGHTING);
}

void SceneRenderer::SetupMaterialFromRGB(float r, float g, float b) {
  m_controller.SetGLColor(r, g, b);
}
