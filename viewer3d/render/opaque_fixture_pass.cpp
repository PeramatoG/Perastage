#include "opaque_fixture_pass.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

#include "matrixutils.h"
#include "configmanager.h"
#include "logger.h"
#include "mesh.h"
#include "opaque_pass_utils.h"
#include "universe_color.h"
#include "perastage_svg_symbol_builder.h"
#include "scenedatamanager.h"
#include "symbols/PerastageSvgSymbol.h"
#include "viewer3dcontroller.h"
#include "gdtfloader.h"

namespace {
namespace fs = std::filesystem;

std::string NormalizePathSeparators(const std::string &path) {
  std::string out = path;
  const char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  std::replace(out.begin(), out.end(), '/', sep);
  return out;
}

std::string NormalizeModelKeyPath(const std::string &path) {
  if (path.empty())
    return {};
  fs::path normalized(path);
  normalized = normalized.lexically_normal();
  return NormalizePathSeparators(normalized.string());
}

std::string BuildFixtureSymbolModelKey(const Fixture &fixture,
                                       const std::string &resolvedGdtfPath) {
  std::string modelKey = NormalizeModelKeyPath(resolvedGdtfPath);
  if (modelKey.empty() && !fixture.typeName.empty())
    modelKey = fixture.typeName;
  if (modelKey.empty())
    modelKey = "unknown";
  return modelKey;
}

const Mesh &FallbackFixtureCubeMesh() {
  static const Mesh mesh = []() {
    Mesh cube;
    cube.vertices = {
        -0.5f, -0.5f, -0.5f, // 0
        0.5f,  -0.5f, -0.5f, // 1
        0.5f,  0.5f,  -0.5f, // 2
        -0.5f, 0.5f,  -0.5f, // 3
        -0.5f, -0.5f, 0.5f,  // 4
        0.5f,  -0.5f, 0.5f,  // 5
        0.5f,  0.5f,  0.5f,  // 6
        -0.5f, 0.5f,  0.5f   // 7
    };
    cube.indices = {
        0, 1, 2, 0, 2, 3, // back
        4, 6, 5, 4, 7, 6, // front
        0, 4, 5, 0, 5, 1, // bottom
        3, 2, 6, 3, 6, 7, // top
        0, 3, 7, 0, 7, 4, // left
        1, 5, 6, 1, 6, 2  // right
    };
    ComputeNormals(cube);
    return cube;
  }();
  return mesh;
}

struct SvgSymbolCacheKey {
  std::string gdtfPath;
  SymbolViewKind viewKind = SymbolViewKind::Top;

  bool operator==(const SvgSymbolCacheKey &other) const {
    return gdtfPath == other.gdtfPath && viewKind == other.viewKind;
  }
};

struct SvgSymbolCacheKeyHasher {
  size_t operator()(const SvgSymbolCacheKey &key) const {
    return std::hash<std::string>{}(key.gdtfPath) ^
           (static_cast<size_t>(key.viewKind) << 1);
  }
};

uint32_t BuildFixtureSymbolStyleVersion(float r, float g, float b) {
  auto toByte = [](float value) -> uint32_t {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint32_t>(std::lround(clamped * 255.0f));
  };
  return 0x1000000u | (toByte(r) << 16) | (toByte(g) << 8) | toByte(b);
}

std::vector<SymbolViewKind> BuildSymbolViewCandidates(SymbolViewKind requested) {
  if (requested == SymbolViewKind::Top)
    return {SymbolViewKind::Top, SymbolViewKind::Bottom};
  if (requested == SymbolViewKind::Bottom)
    return {SymbolViewKind::Bottom, SymbolViewKind::Top};
  if (requested == SymbolViewKind::Front)
    return {SymbolViewKind::Front, SymbolViewKind::Top};
  return {requested};
}

std::array<float, 3> BuildSvgVertexForView(float x, float y, Viewer2DView view);

struct SvgTessellationContext {
  std::vector<std::array<GLdouble, 3>> generatedVertices;
};

void TessBeginCallback(GLenum type, void *polygonData) {
  (void)polygonData;
  glBegin(type);
}

void TessVertexCallback(void *vertexData, void *polygonData) {
  (void)polygonData;
  const auto *vertex = static_cast<const GLdouble *>(vertexData);
  glVertex3dv(vertex);
}

void TessEndCallback(void *polygonData) {
  (void)polygonData;
  glEnd();
}

void TessCombineCallback(GLdouble coords[3], void *vertexData[4],
                                  GLfloat weight[4], void **outData,
                                  void *polygonData) {
  (void)vertexData;
  (void)weight;
  auto *context = static_cast<SvgTessellationContext *>(polygonData);
  context->generatedVertices.push_back({coords[0], coords[1], coords[2]});
  *outData = context->generatedVertices.back().data();
}

void TessErrorCallback(GLenum errorCode, void *polygonData) {
  (void)polygonData;
  (void)errorCode;
}

void AppendSvgContourVertices(std::vector<std::array<GLdouble, 3>> &storage,
                              const std::vector<PerastageSvgPoint> &points,
                              const PerastageSvgSymbolData &svg,
                              Viewer2DView view, double anchorX,
                              double anchorY) {
  if (points.size() < 3)
    return;
  storage.reserve(storage.size() + points.size());
  for (const auto &point : points) {
    const auto vertex = BuildSvgVertexForView(
        static_cast<float>(point.x + svg.offsetXmm - anchorX),
        static_cast<float>(point.y + svg.offsetYmm - anchorY), view);
    storage.push_back({static_cast<GLdouble>(vertex[0]),
                       static_cast<GLdouble>(vertex[1]),
                       static_cast<GLdouble>(vertex[2])});
  }
}

void DrawSvgFilledPolygon(const PerastageSvgPolygon &polygon,
                          const PerastageSvgSymbolData &svg,
                          Viewer2DView view, double anchorX,
                          double anchorY) {
  if (polygon.points.size() < 3)
    return;

  GLUtesselator *tess = gluNewTess();
  if (!tess)
    return;

  gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
  gluTessCallback(tess, GLU_TESS_BEGIN_DATA,
                  reinterpret_cast<void (*)()>(TessBeginCallback));
  gluTessCallback(tess, GLU_TESS_VERTEX_DATA,
                  reinterpret_cast<void (*)()>(TessVertexCallback));
  gluTessCallback(tess, GLU_TESS_END_DATA,
                  reinterpret_cast<void (*)()>(TessEndCallback));
  gluTessCallback(tess, GLU_TESS_COMBINE_DATA,
                  reinterpret_cast<void (*)()>(TessCombineCallback));
  gluTessCallback(tess, GLU_TESS_ERROR_DATA,
                  reinterpret_cast<void (*)()>(TessErrorCallback));

  SvgTessellationContext context;
  std::vector<std::array<GLdouble, 3>> contourVertices;
  AppendSvgContourVertices(contourVertices, polygon.points, svg, view, anchorX,
                           anchorY);
  for (const auto &hole : polygon.holes)
    AppendSvgContourVertices(contourVertices, hole, svg, view, anchorX, anchorY);

  if (contourVertices.size() < polygon.points.size()) {
    gluDeleteTess(tess);
    return;
  }

  size_t contourOffset = 0;
  gluTessBeginPolygon(tess, &context);
  auto emitContour = [&](const std::vector<PerastageSvgPoint> &points) {
    if (points.size() < 3)
      return;
    gluTessBeginContour(tess);
    for (size_t i = 0; i < points.size(); ++i) {
      GLdouble *vertex = contourVertices[contourOffset + i].data();
      gluTessVertex(tess, vertex, vertex);
    }
    gluTessEndContour(tess);
    contourOffset += points.size();
  };

  emitContour(polygon.points);
  for (const auto &hole : polygon.holes)
    emitContour(hole);
  gluTessEndPolygon(tess);

  gluDeleteTess(tess);
}

std::array<float, 3> BuildSvgVertexForView(float x, float y, Viewer2DView view) {
  switch (view) {
  case Viewer2DView::Front:
    return {x, 0.0f, y};
  case Viewer2DView::Side:
    return {0.0f, -x, y};
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
  default:
    return {x, y, 0.0f};
  }
}

static int ParseUniverseFromAddress(const std::string &address) {
  if (address.empty())
    return -1;
  const size_t dot = address.find('.');
  if (dot == std::string::npos)
    return -1;
  int u = 0;
  for (size_t i = 0; i < dot; ++i) {
    if (address[i] < '0' || address[i] > '9')
      return -1;
    u = u * 10 + (address[i] - '0');
  }
  return u > 0 ? u : -1;
}

bool DrawPerastageSvgInFixturePass(const PerastageSvgSymbolData &svg,
                                   Viewer2DView view, float fillR, float fillG,
                                   float fillB) {
  if (!svg.IsValid())
    return false;

  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
  bool hasBounds = false;
  auto includePoint = [&](const PerastageSvgPoint &point) {
    const double px = point.x + svg.offsetXmm;
    const double py = point.y + svg.offsetYmm;
    if (!hasBounds) {
      minX = maxX = px;
      minY = maxY = py;
      hasBounds = true;
      return;
    }
    minX = std::min(minX, px);
    minY = std::min(minY, py);
    maxX = std::max(maxX, px);
    maxY = std::max(maxY, py);
  };

  for (const auto &polygon : svg.fills) {
    for (const auto &point : polygon.points)
      includePoint(point);
    for (const auto &hole : polygon.holes) {
      for (const auto &point : hole)
        includePoint(point);
    }
  }
  for (const auto &line : svg.strokes) {
    for (const auto &point : line.points)
      includePoint(point);
  }

  const double anchorX = hasBounds ? (minX + maxX) * 0.5 : 0.0;
  const bool useTopHangAnchor =
      view == Viewer2DView::Front || view == Viewer2DView::Side;
  const double anchorY =
      hasBounds ? (useTopHangAnchor ? maxY : (minY + maxY) * 0.5) : 0.0;

  glPushMatrix();
  glScalef(RENDER_SCALE, RENDER_SCALE, RENDER_SCALE);

  glColor3f(fillR, fillG, fillB);
  for (const auto &polygon : svg.fills)
    DrawSvgFilledPolygon(polygon, svg, view, anchorX, anchorY);

  glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1.0f);
  for (const auto &line : svg.strokes) {
    if (line.points.size() < 2)
      continue;
    glBegin(GL_LINE_STRIP);
    for (const auto &point : line.points) {
      const auto vertex =
          BuildSvgVertexForView(static_cast<float>(point.x + svg.offsetXmm - anchorX),
                                static_cast<float>(point.y + svg.offsetYmm - anchorY),
                                view);
      glVertex3f(vertex[0], vertex[1], vertex[2]);
    }
    glEnd();
  }

  glPopMatrix();
  return true;
}

void CancelFixtureRotationForLayoutSvg(const Matrix &fixtureTransform,
                                       Viewer2DView view) {
  if (view != Viewer2DView::Front && view != Viewer2DView::Side)
    return;

  float inverseRotation[16] = {
      fixtureTransform.u[0], fixtureTransform.v[0], fixtureTransform.w[0], 0.0f,
      fixtureTransform.u[1], fixtureTransform.v[1], fixtureTransform.w[1], 0.0f,
      fixtureTransform.u[2], fixtureTransform.v[2], fixtureTransform.w[2], 0.0f,
      0.0f,                 0.0f,                 0.0f,                 1.0f,
  };
  glMultMatrixf(inverseRotation);
}

void DrawBoundsSolid(const Viewer3DBoundingBox &bb) {
  glBegin(GL_QUADS);
  glVertex3f(bb.min[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.min[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.min[0], bb.min[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.min[2]);
  glVertex3f(bb.max[0], bb.max[1], bb.max[2]);
  glVertex3f(bb.max[0], bb.min[1], bb.max[2]);
  glEnd();
}

struct FixtureInstancedBatchKey {
  const Mesh *mesh = nullptr;
  uint32_t colorStyle = 0;
  bool unlit = false;
  bool wireframe = false;
  Viewer2DRenderMode mode = Viewer2DRenderMode::White;
  float scale = RENDER_SCALE;

  bool operator==(const FixtureInstancedBatchKey &other) const {
    return mesh == other.mesh && colorStyle == other.colorStyle &&
           unlit == other.unlit && wireframe == other.wireframe &&
           mode == other.mode && scale == other.scale;
  }
};

struct FixtureInstancedBatchKeyHasher {
  size_t operator()(const FixtureInstancedBatchKey &key) const {
    size_t h = std::hash<const Mesh *>{}(key.mesh);
    h ^= (static_cast<size_t>(key.colorStyle) << 1);
    h ^= (static_cast<size_t>(key.unlit) << 8);
    h ^= (static_cast<size_t>(key.wireframe) << 9);
    h ^= (static_cast<size_t>(key.mode) << 10);
    h ^= (std::hash<float>{}(key.scale) << 11);
    return h;
  }
};

struct FixtureInstancedDrawCall {
  float cx = 0.0f;
  float cy = 0.0f;
  float cz = 0.0f;
  std::array<float, 16> fixtureMatrix{};
  std::array<float, 16> localMatrix{};
  bool hasLocalMatrix = false;
  std::array<float, 16> worldMatrix{};
};

using FixtureInstancedBatches =
    std::unordered_map<FixtureInstancedBatchKey,
                       std::vector<FixtureInstancedDrawCall>,
                       FixtureInstancedBatchKeyHasher>;

struct FixtureRenderMetrics {
  size_t instancedFixtures = 0;
  size_t fallbackFixtures = 0;
  size_t instancedDrawCalls = 0;
  size_t fallbackDrawCalls = 0;
};

struct FixtureInstancedProgram {
  GLuint program = 0;
  GLint position = -1;
  GLint normal = -1;
  GLint instanceCol0 = -1;
  GLint instanceCol1 = -1;
  GLint instanceCol2 = -1;
  GLint instanceCol3 = -1;
  GLint viewProjection = -1;
  GLint color = -1;
  GLint unlit = -1;
  GLint lightDir = -1;
  GLint scale = -1;
};

GLuint CompileFixtureShader(GLenum type, const char *source) {
  const GLuint shader = glCreateShader(type);
  if (shader == 0)
    return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled != GL_TRUE) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

const FixtureInstancedProgram &GetFixtureInstancedProgram() {
  static const FixtureInstancedProgram program = []() {
    FixtureInstancedProgram p;
    static constexpr const char *kVs = R"glsl(
      #version 120
      attribute vec3 aPosition;
      attribute vec3 aNormal;
      attribute vec4 aInstanceCol0;
      attribute vec4 aInstanceCol1;
      attribute vec4 aInstanceCol2;
      attribute vec4 aInstanceCol3;
      uniform mat4 uViewProjection;
      uniform float uScale;
      varying vec3 vNormal;
      void main() {
        mat4 instance = mat4(aInstanceCol0, aInstanceCol1, aInstanceCol2, aInstanceCol3);
        vec4 worldPos = instance * vec4(aPosition * uScale, 1.0);
        gl_Position = uViewProjection * worldPos;
        vNormal = mat3(instance) * aNormal;
      }
    )glsl";
    static constexpr const char *kFs = R"glsl(
      #version 120
      uniform vec3 uColor;
      uniform float uUnlit;
      uniform vec3 uLightDir;
      varying vec3 vNormal;
      void main() {
        float ndotl = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
        float lightFactor = mix(0.25, 1.0, ndotl);
        vec3 shaded = mix(uColor * lightFactor, uColor, uUnlit);
        gl_FragColor = vec4(shaded, 1.0);
      }
    )glsl";

    const GLuint vs = CompileFixtureShader(GL_VERTEX_SHADER, kVs);
    const GLuint fs = CompileFixtureShader(GL_FRAGMENT_SHADER, kFs);
    if (vs == 0 || fs == 0) {
      if (vs != 0)
        glDeleteShader(vs);
      if (fs != 0)
        glDeleteShader(fs);
      return p;
    }
    p.program = glCreateProgram();
    if (p.program == 0) {
      glDeleteShader(vs);
      glDeleteShader(fs);
      return p;
    }
    glAttachShader(p.program, vs);
    glAttachShader(p.program, fs);
    glBindAttribLocation(p.program, 0, "aPosition");
    glBindAttribLocation(p.program, 1, "aNormal");
    glBindAttribLocation(p.program, 2, "aInstanceCol0");
    glBindAttribLocation(p.program, 3, "aInstanceCol1");
    glBindAttribLocation(p.program, 4, "aInstanceCol2");
    glBindAttribLocation(p.program, 5, "aInstanceCol3");
    glLinkProgram(p.program);
    GLint linked = GL_FALSE;
    glGetProgramiv(p.program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (linked != GL_TRUE) {
      glDeleteProgram(p.program);
      p.program = 0;
      return p;
    }
    p.position = glGetAttribLocation(p.program, "aPosition");
    p.normal = glGetAttribLocation(p.program, "aNormal");
    p.instanceCol0 = glGetAttribLocation(p.program, "aInstanceCol0");
    p.instanceCol1 = glGetAttribLocation(p.program, "aInstanceCol1");
    p.instanceCol2 = glGetAttribLocation(p.program, "aInstanceCol2");
    p.instanceCol3 = glGetAttribLocation(p.program, "aInstanceCol3");
    p.viewProjection = glGetUniformLocation(p.program, "uViewProjection");
    p.color = glGetUniformLocation(p.program, "uColor");
    p.unlit = glGetUniformLocation(p.program, "uUnlit");
    p.lightDir = glGetUniformLocation(p.program, "uLightDir");
    p.scale = glGetUniformLocation(p.program, "uScale");
    return p;
  }();
  return program;
}

bool TryRenderBatchInstanced(const FixtureInstancedBatchKey &key,
                             const std::vector<FixtureInstancedDrawCall> &draws) {
  if (draws.empty() || key.wireframe || key.mode == Viewer2DRenderMode::Wireframe)
    return false;
  const Mesh &mesh = *key.mesh;
  if (!mesh.buffersReady || mesh.vao == 0 || mesh.vboVertices == 0 ||
      mesh.vboNormals == 0 || mesh.eboTriangles == 0 || mesh.triangleIndexCount <= 0)
    return false;
  if (!(GLEW_VERSION_3_1 || GLEW_ARB_draw_instanced) ||
      !(GLEW_VERSION_3_3 || GLEW_ARB_instanced_arrays))
    return false;

  const FixtureInstancedProgram &program = GetFixtureInstancedProgram();
  if (program.program == 0 || program.position < 0 || program.normal < 0 ||
      program.instanceCol0 < 0 || program.instanceCol1 < 0 ||
      program.instanceCol2 < 0 || program.instanceCol3 < 0)
    return false;

  std::vector<float> instanceMatrices;
  instanceMatrices.reserve(draws.size() * 16);
  for (const auto &draw : draws)
    instanceMatrices.insert(instanceMatrices.end(), draw.worldMatrix.begin(),
                            draw.worldMatrix.end());

  float modelView[16];
  float projection[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
  glGetFloatv(GL_PROJECTION_MATRIX, projection);
  float viewProjection[16]{};
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      viewProjection[c * 4 + r] = projection[0 * 4 + r] * modelView[c * 4 + 0] +
                                  projection[1 * 4 + r] * modelView[c * 4 + 1] +
                                  projection[2 * 4 + r] * modelView[c * 4 + 2] +
                                  projection[3 * 4 + r] * modelView[c * 4 + 3];
    }
  }

  GLuint instanceBuffer = 0;
  glGenBuffers(1, &instanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
  glBufferData(GL_ARRAY_BUFFER, instanceMatrices.size() * sizeof(float),
               instanceMatrices.data(), GL_STREAM_DRAW);

  const uint32_t color = key.colorStyle;
  const float r = static_cast<float>((color >> 16) & 0xFFu) / 255.0f;
  const float g = static_cast<float>((color >> 8) & 0xFFu) / 255.0f;
  const float b = static_cast<float>(color & 0xFFu) / 255.0f;

  GLint previousProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glUseProgram(program.program);
  glUniformMatrix4fv(program.viewProjection, 1, GL_FALSE, viewProjection);
  glUniform3f(program.color, r, g, b);
  glUniform1f(program.unlit, key.unlit ? 1.0f : 0.0f);
  glUniform3f(program.lightDir, 0.35f, -0.55f, 1.0f);
  glUniform1f(program.scale, key.scale);

  glBindVertexArray(mesh.vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vboVertices);
  glEnableVertexAttribArray(static_cast<GLuint>(program.position));
  glVertexAttribPointer(static_cast<GLuint>(program.position), 3, GL_FLOAT,
                        GL_FALSE, 0, nullptr);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
  glEnableVertexAttribArray(static_cast<GLuint>(program.normal));
  glVertexAttribPointer(static_cast<GLuint>(program.normal), 3, GL_FLOAT, GL_FALSE,
                        0, nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
  constexpr GLsizei kStride = sizeof(float) * 16;
  glEnableVertexAttribArray(static_cast<GLuint>(program.instanceCol0));
  glEnableVertexAttribArray(static_cast<GLuint>(program.instanceCol1));
  glEnableVertexAttribArray(static_cast<GLuint>(program.instanceCol2));
  glEnableVertexAttribArray(static_cast<GLuint>(program.instanceCol3));
  glVertexAttribPointer(static_cast<GLuint>(program.instanceCol0), 4, GL_FLOAT,
                        GL_FALSE, kStride, reinterpret_cast<void *>(0));
  glVertexAttribPointer(static_cast<GLuint>(program.instanceCol1), 4, GL_FLOAT,
                        GL_FALSE, kStride, reinterpret_cast<void *>(sizeof(float) * 4));
  glVertexAttribPointer(static_cast<GLuint>(program.instanceCol2), 4, GL_FLOAT,
                        GL_FALSE, kStride, reinterpret_cast<void *>(sizeof(float) * 8));
  glVertexAttribPointer(static_cast<GLuint>(program.instanceCol3), 4, GL_FLOAT,
                        GL_FALSE, kStride, reinterpret_cast<void *>(sizeof(float) * 12));
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol0), 1);
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol1), 1);
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol2), 1);
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol3), 1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);
  glDrawElementsInstanced(GL_TRIANGLES, mesh.triangleIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(draws.size()));

  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol0), 0);
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol1), 0);
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol2), 0);
  glVertexAttribDivisor(static_cast<GLuint>(program.instanceCol3), 0);
  glDisableVertexAttribArray(static_cast<GLuint>(program.instanceCol3));
  glDisableVertexAttribArray(static_cast<GLuint>(program.instanceCol2));
  glDisableVertexAttribArray(static_cast<GLuint>(program.instanceCol1));
  glDisableVertexAttribArray(static_cast<GLuint>(program.instanceCol0));
  glDisableVertexAttribArray(static_cast<GLuint>(program.normal));
  glDisableVertexAttribArray(static_cast<GLuint>(program.position));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &instanceBuffer);
  glUseProgram(static_cast<GLuint>(previousProgram));
  return true;
}

void AddFixtureInstancedDraw(FixtureInstancedBatches &batches, const Mesh &mesh,
                             float r, float g, float b, bool unlit,
                             bool wireframe, Viewer2DRenderMode mode,
                             float scale, float cx, float cy, float cz,
                             const float *fixtureMatrix,
                             const float *localMatrix,
                             const float *worldMatrix) {
  FixtureInstancedBatchKey key;
  key.mesh = &mesh;
  key.colorStyle = BuildFixtureSymbolStyleVersion(r, g, b);
  key.unlit = unlit;
  key.wireframe = wireframe;
  key.mode = mode;
  key.scale = scale;

  FixtureInstancedDrawCall draw;
  draw.cx = cx;
  draw.cy = cy;
  draw.cz = cz;
  for (size_t i = 0; i < draw.fixtureMatrix.size(); ++i)
    draw.fixtureMatrix[i] = fixtureMatrix[i];
  if (localMatrix) {
    draw.hasLocalMatrix = true;
    for (size_t i = 0; i < draw.localMatrix.size(); ++i)
      draw.localMatrix[i] = localMatrix[i];
  }
  for (size_t i = 0; i < draw.worldMatrix.size(); ++i)
    draw.worldMatrix[i] = worldMatrix[i];

  batches[key].push_back(std::move(draw));
}

} // namespace

void OpaqueFixturePass::Render(
    Viewer3DController &controller, const RenderFrameContext &context,
    const Viewer3DVisibleSet &visibleSet,
    const std::function<std::array<float, 3>(const std::string &, const std::string &)> &getTypeColor,
    const std::function<std::array<float, 3>(const std::string &)> &getLayerColor,
    const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView,
    const std::function<std::array<float, 3>(const std::string &)> &getPickColor) {
  if (context.idOnlyPass) {
    glShadeModel(GL_FLAT);
    for (const auto &uuid : visibleSet.fixtureUuids) {
      auto bbIt = controller.m_fixtureBounds.find(uuid);
      if (bbIt == controller.m_fixtureBounds.end())
        continue;
      const auto pickColor = getPickColor(uuid);
      glColor3f(pickColor[0], pickColor[1], pickColor[2]);
      DrawBoundsSolid(bbIt->second);
    }
    return;
  }
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const bool skipCapture = context.skipCapture;
  const bool is2DViewer = context.is2DViewer;
  const bool isTopView2D = is2DViewer && context.view == Viewer2DView::Top;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  std::unordered_map<SvgSymbolCacheKey, std::optional<PerastageSvgSymbolData>,
                     SvgSymbolCacheKeyHasher>
      perastageSvgCache;

  // Top-view fixtures support two drawing modes:
  // - natural top view (real top)
  // - forced bottom view (for hanging rigs)
  // This override is intentionally fixture-only.
  bool forceBottomViewForTopFixtures =
      ConfigManager::Get().GetFloat("view2d_top_fixtures_inverted") != 0.0f;
  if (controller.GetForceBottomViewForTopFixturesOverride().has_value()) {
    forceBottomViewForTopFixtures =
        controller.GetForceBottomViewForTopFixturesOverride().value();
  }
  const bool drawRealTopInTopView = isTopView2D && !forceBottomViewForTopFixtures;

  glShadeModel((context.texturedStyle && !wireframe) ? GL_SMOOTH : GL_FLAT);
  // Keep 2D wireframe fixture overlays unchanged, but in the real 3D wireframe
  // viewer fixtures must respect depth like all other geometry.
  const bool forceFixturesOnTop = wireframe && is2DViewer;
  GLboolean depthEnabled = GL_FALSE;
  if (forceFixturesOnTop) {
    depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled)
      glDisable(GL_DEPTH_TEST);
  }
  FixtureInstancedBatches fixtureInstancedBatches;
  FixtureRenderMetrics frameMetrics;
  auto RenderFixtureInstancedBatches =
      [&](const FixtureInstancedBatches &batches) {
        size_t drawCalls = 0;
        for (const auto &entry : batches) {
          const auto &key = entry.first;
          const auto &draws = entry.second;
          // True GPU instancing path: when possible, render all fixtures that
          // share the same mesh/material state in a single
          // glDrawElementsInstanced call.
          if (!context.texturedStyle && TryRenderBatchInstanced(key, draws)) {
            ++drawCalls;
            continue;
          }
          for (const auto &draw : draws) {
            glPushMatrix();
            controller.ApplyTransform(draw.fixtureMatrix.data(), true);
            if (draw.hasLocalMatrix)
              controller.ApplyTransform(draw.localMatrix.data(), false);
            const uint32_t color = key.colorStyle;
            const float r = static_cast<float>((color >> 16) & 0xFFu) / 255.0f;
            const float g = static_cast<float>((color >> 8) & 0xFFu) / 255.0f;
            const float b = static_cast<float>(color & 0xFFu) / 255.0f;
            controller.DrawMeshWithOutline(
                *key.mesh, r, g, b, key.scale, false, false, draw.cx, draw.cy,
                draw.cz, key.wireframe, key.mode,
                [](const std::array<float, 3> &p) { return p; }, key.unlit,
                draw.worldMatrix.data());
            glPopMatrix();
            ++drawCalls;
          }
        }
        return drawCalls;
      };
  for (const auto &uuid : visibleSet.fixtureUuids) {
    auto fixtureIt = fixtures.find(uuid);
    if (fixtureIt == fixtures.end())
      continue;
    const auto &f = fixtureIt->second;
    glPushMatrix();

    std::string fixtureCaptureKey;
    if (controller.m_captureCanvas && !skipCapture) {
      fixtureCaptureKey = !f.typeName.empty()
                              ? f.typeName
                              : (!f.gdtfSpec.empty() ? f.gdtfSpec : "unknown");
      controller.m_captureCanvas->SetSourceKey(fixtureCaptureKey);
    }

    const bool highlight =
        context.selectionOverlayPass && !controller.m_highlightUuid.empty() &&
        uuid == controller.m_highlightUuid;
    const bool selected = context.selectionOverlayPass &&
                          controller.m_selectedUuids.find(uuid) !=
                              controller.m_selectedUuids.end();

    float matrix[16];
    MatrixToArray(f.transform, matrix);
    controller.ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto fbit = controller.m_fixtureBounds.find(uuid);
    if (fbit != controller.m_fixtureBounds.end()) {
      cx = (fbit->second.min[0] + fbit->second.max[0]) * 0.5f;
      cy = (fbit->second.min[1] + fbit->second.max[1]) * 0.5f;
      cz = (fbit->second.min[2] + fbit->second.max[2]) * 0.5f;
      cx -= f.transform.o[0] * RENDER_SCALE;
      cy -= f.transform.o[1] * RENDER_SCALE;
      cz -= f.transform.o[2] * RENDER_SCALE;
    }

    std::string gdtfPath;
    auto gdtfPathIt = controller.m_resourceSyncState.resolvedGdtfSpecs.find(
        ResolveCacheKey(f.gdtfSpec));
    if (gdtfPathIt != controller.m_resourceSyncState.resolvedGdtfSpecs.end() &&
        gdtfPathIt->second.attempted)
      gdtfPath = gdtfPathIt->second.resolvedPath;

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (context.whiteModelStyle && !wireframe) {
      r = 0.95f;
      g = 0.95f;
      b = 0.95f;
    } else if (context.texturedStyle && !wireframe) {
      // Keep textured fixtures on a neutral tone. Colorized modes such as
      // ByFixtureType override this with fixture-driven colors afterwards.
      r = 0.2f;
      g = 0.2f;
      b = 0.2f;
    }
    if (mode == Viewer2DRenderMode::Wireframe) {
      auto c = getLayerColor(f.layer);
      r = c[0];
      g = c[1];
      b = c[2];
    } else if (mode == Viewer2DRenderMode::ByFixtureType) {
      auto c = getTypeColor(f.gdtfSpec, f.color);
      r = c[0];
      g = c[1];
      b = c[2];
    } else if (mode == Viewer2DRenderMode::ByLayer) {
      auto c = getLayerColor(f.layer);
      r = c[0];
      g = c[1];
      b = c[2];
    } else if (mode == Viewer2DRenderMode::ByUniverse) {
      auto c = GetUniverseColor(ParseUniverseFromAddress(f.address));
      r = c[0];
      g = c[1];
      b = c[2];
    }

    Matrix fixtureTransform = f.transform;
    fixtureTransform.o[0] *= RENDER_SCALE;
    fixtureTransform.o[1] *= RENDER_SCALE;
    fixtureTransform.o[2] *= RENDER_SCALE;

    auto applyFixtureCapture = [fixtureTransform](const std::array<float, 3> &p) {
      return TransformPoint(fixtureTransform, p);
    };

    const std::string normalizedGdtfPath = NormalizeModelKeyPath(gdtfPath);
    const std::string modelKey =
        BuildFixtureSymbolModelKey(f, normalizedGdtfPath);
    const std::string svgSourcePath = normalizedGdtfPath;

    auto itg = controller.m_resourceSyncState.loadedGdtf.find(gdtfPath);

    bool renderedPerastageSvg = false;
    const bool captureRecordingActive = controller.m_captureCanvas && !skipCapture;
    const bool preferLayoutSvg =
        context.preferPerastageSvgSymbolsForLayouts && is2DViewer &&
        !captureRecordingActive && mode != Viewer2DRenderMode::Wireframe;
    if (preferLayoutSvg && !svgSourcePath.empty()) {
      const Viewer2DView fixtureView =
          isTopView2D && forceBottomViewForTopFixtures ? Viewer2DView::Bottom
                                                       : context.view;
      const SymbolViewKind requestedView = resolveSymbolView(fixtureView);
      const std::vector<SymbolViewKind> candidates =
          BuildSymbolViewCandidates(requestedView);
      for (SymbolViewKind candidateView : candidates) {
        const SvgSymbolCacheKey cacheKey{svgSourcePath, candidateView};
        auto cacheIt = perastageSvgCache.find(cacheKey);
        if (cacheIt == perastageSvgCache.end()) {
          std::optional<PerastageSvgSymbolData> loaded;
          PerastageSvgSymbolData svg;
          if (LoadPerastageSvgSymbolFromGdtf(svgSourcePath, candidateView, svg))
            loaded = std::move(svg);
          cacheIt = perastageSvgCache.emplace(cacheKey, std::move(loaded)).first;
        }
        if (!cacheIt->second.has_value())
          continue;
        CancelFixtureRotationForLayoutSvg(f.transform, context.view);
        renderedPerastageSvg = DrawPerastageSvgInFixturePass(
            cacheIt->second.value(), context.view, r, g, b);
        if (renderedPerastageSvg)
          break;
      }
    }

    const bool useSymbolInstancing =
        (controller.m_captureUseSymbols &&
         (controller.m_captureView == Viewer2DView::Bottom ||
          controller.m_captureView == Viewer2DView::Top ||
          controller.m_captureView == Viewer2DView::Front ||
          controller.m_captureView == Viewer2DView::Side) &&
         mode != Viewer2DRenderMode::Wireframe &&
         !highlight && !selected);
    if (useSymbolInstancing && controller.m_captureCanvas && !skipCapture) {
      if (!modelKey.empty()) {
        const Viewer2DView fixtureCaptureView =
            isTopView2D && forceBottomViewForTopFixtures
                ? Viewer2DView::Bottom
                : controller.m_captureView;

        SymbolKey symbolKey;
        symbolKey.modelKey = modelKey;
        symbolKey.viewKind = resolveSymbolView(fixtureCaptureView);
        symbolKey.styleVersion = BuildFixtureSymbolStyleVersion(r, g, b);

        const auto &symbol =
            controller.m_bottomSymbolCache.GetOrCreate(symbolKey, [&](const SymbolKey &,
                                                         uint32_t symbolId) {
              SymbolDefinition svgDefinition{};
              if (!svgSourcePath.empty() &&
                  TryBuildPerastageSvgSymbolDefinition(svgSourcePath,
                                                       symbolKey.viewKind,
                                                       symbolId,
                                                       {r, g, b},
                                                       svgDefinition)) {
                return svgDefinition;
              }

              SymbolDefinition definition{};
              definition.symbolId = symbolId;
              auto localCanvas =
                  CreateRecordingCanvas(definition.localCommands, false);
              CanvasTransform transform{};
              localCanvas->BeginFrame();
              localCanvas->SetTransform(transform);

              ICanvas2D *prevCanvas = controller.m_captureCanvas;
              Viewer2DView prevView = controller.m_captureView;
              bool prevCaptureOnly = controller.m_captureOnly;
              bool prevIncludeGrid = controller.m_captureIncludeGrid;
              controller.m_captureCanvas = localCanvas.get();
              controller.m_captureView = fixtureCaptureView;
              controller.m_captureOnly = true;
              controller.m_captureIncludeGrid = false;

              if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
                const auto &parts = itg->second;
                const bool reversePartOrder = drawRealTopInTopView;
                for (size_t offset = 0; offset < parts.size(); ++offset) {
                  const size_t partIndex =
                      reversePartOrder ? (parts.size() - 1 - offset) : offset;
                  const auto &obj = parts[partIndex];
                  controller.m_captureCanvas->SetSourceKey(
                      fixtureCaptureKey + "_part" + std::to_string(partIndex));
                  auto applyCapture = [objTransform = obj.transform](
                                          const std::array<float, 3> &p) {
                    return TransformPoint(objTransform, p);
                  };
                  float partR = r;
                  float partG = g;
                  float partB = b;
                  if (!is2DViewer && obj.isLens) {
                    const bool isWhiteRenderMode =
                        controller.IsPureWhiteRenderStyleEnabled();
                    partR = 1.0f;
                    partG = isWhiteRenderMode ? 1.0f : 0.78f;
                    partB = isWhiteRenderMode ? 1.0f : 0.35f;
                  }
                  controller.DrawMeshWithOutline(
                      obj.mesh, partR, partG, partB, RENDER_SCALE, false, false,
                      0.0f, 0.0f, 0.0f, wireframe, mode, applyCapture, false);
                }
              } else {
                controller.m_captureCanvas->SetSourceKey(fixtureCaptureKey);
                const bool fallbackWireframe =
                    wireframe;
                const bool fallbackUnlit =
                    context.whiteModelStyle &&
                    !controller.IsSketchRenderStyleEnabled();
                controller.DrawMeshWithOutline(
                    FallbackFixtureCubeMesh(), r, g, b, 0.2f, false, false,
                    0.0f, 0.0f, 0.0f, fallbackWireframe, mode,
                    [](const std::array<float, 3> &p) { return p; },
                    fallbackUnlit);
              }

              localCanvas->EndFrame();
              definition.bounds = ComputeSymbolBounds(definition.localCommands);

              controller.m_captureCanvas = prevCanvas;
              controller.m_captureView = prevView;
              controller.m_captureOnly = prevCaptureOnly;
              controller.m_captureIncludeGrid = prevIncludeGrid;
              return definition;
            });

        Transform2D instanceTransform =
            BuildInstanceTransform2D(fixtureTransform, fixtureCaptureView);
        controller.m_captureCanvas->PlaceSymbolInstance(symbol.symbolId,
                                                        instanceTransform);
      }
    }

    auto drawFixtureGeometry = [&]() {
      size_t drawCalls = 0;
      if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
        const auto &parts = itg->second;
        const bool reversePartOrder = drawRealTopInTopView;
        for (size_t offset = 0; offset < parts.size(); ++offset) {
          const size_t partIndex =
              reversePartOrder ? (parts.size() - 1 - offset) : offset;
          const auto &obj = parts[partIndex];
          glPushMatrix();
          if (controller.m_captureCanvas && !skipCapture) {
            controller.m_captureCanvas->SetSourceKey(
                fixtureCaptureKey + "_part" + std::to_string(partIndex));
          }
          float m2[16];
          MatrixToArray(obj.transform, m2);
          controller.ApplyTransform(m2, false);
          Matrix worldMatrix = MatrixUtils::Multiply(f.transform, obj.transform);
          float partMatrix[16];
          MatrixToArray(worldMatrix, partMatrix);
          auto applyCapture =
              [fixtureTransform, objTransform = obj.transform](
                  const std::array<float, 3> &p) {
                auto local = TransformPoint(objTransform, p);
                return TransformPoint(fixtureTransform, local);
              };
          float partR = r;
          float partG = g;
          float partB = b;
          if (!is2DViewer && obj.isLens) {
            const bool isWhiteRenderMode =
                controller.IsPureWhiteRenderStyleEnabled();
            partR = 1.0f;
            partG = isWhiteRenderMode ? 1.0f : 0.78f;
            partB = isWhiteRenderMode ? 1.0f : 0.35f;
          }
          const bool drawUnlit = !is2DViewer && obj.isLens;
          controller.DrawMeshWithOutline(obj.mesh, partR, partG, partB,
                                         RENDER_SCALE, highlight, selected, cx,
                                         cy, cz, wireframe, mode, applyCapture,
                                         drawUnlit, partMatrix);
          ++drawCalls;
          glPopMatrix();
        }
      } else {
        const bool fallbackWireframe =
            wireframe;
        const bool fallbackUnlit =
            !highlight && !selected &&
            context.whiteModelStyle &&
            !controller.IsSketchRenderStyleEnabled();
        controller.DrawMeshWithOutline(FallbackFixtureCubeMesh(), r, g, b, 0.2f,
                                       highlight, selected, cx, cy, cz,
                                       fallbackWireframe, mode,
                                       applyFixtureCapture, fallbackUnlit,
                                       matrix);
        ++drawCalls;
      }
      return drawCalls;
    };

    if (renderedPerastageSvg) {
      ++frameMetrics.fallbackFixtures;
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
    }

    if (context.skipOptionalWork) {
      ++frameMetrics.instancedFixtures;
      if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
        const auto &parts = itg->second;
        const bool reversePartOrder = drawRealTopInTopView;
        for (size_t offset = 0; offset < parts.size(); ++offset) {
          const size_t partIndex =
              reversePartOrder ? (parts.size() - 1 - offset) : offset;
          const auto &obj = parts[partIndex];
          float localMatrix[16];
          MatrixToArray(obj.transform, localMatrix);
          Matrix worldMatrix = MatrixUtils::Multiply(f.transform, obj.transform);
          float worldMatrixArray[16];
          MatrixToArray(worldMatrix, worldMatrixArray);
          AddFixtureInstancedDraw(fixtureInstancedBatches, obj.mesh, r, g, b, false,
                                  wireframe, mode, RENDER_SCALE, cx, cy, cz,
                                  matrix, localMatrix, worldMatrixArray);
        }
      } else {
        AddFixtureInstancedDraw(fixtureInstancedBatches, FallbackFixtureCubeMesh(),
                                r, g, b, false, wireframe, mode, 0.2f, cx, cy,
                                cz, matrix, nullptr, matrix);
      }
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
    }

    const bool eligibleForFixtureInstancedBatch =
        !highlight && !selected && !captureRecordingActive;
    if (eligibleForFixtureInstancedBatch) {
      ++frameMetrics.instancedFixtures;
      if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
        const auto &parts = itg->second;
        const bool reversePartOrder = drawRealTopInTopView;
        for (size_t offset = 0; offset < parts.size(); ++offset) {
          const size_t partIndex =
              reversePartOrder ? (parts.size() - 1 - offset) : offset;
          const auto &obj = parts[partIndex];
          float localMatrix[16];
          MatrixToArray(obj.transform, localMatrix);
          Matrix worldMatrix = MatrixUtils::Multiply(f.transform, obj.transform);
          float worldMatrixArray[16];
          MatrixToArray(worldMatrix, worldMatrixArray);

          float partR = r;
          float partG = g;
          float partB = b;
          if (!is2DViewer && obj.isLens) {
            const bool isWhiteRenderMode =
                controller.IsPureWhiteRenderStyleEnabled();
            partR = 1.0f;
            partG = isWhiteRenderMode ? 1.0f : 0.78f;
            partB = isWhiteRenderMode ? 1.0f : 0.35f;
          }
          const bool drawUnlit = !is2DViewer && obj.isLens;
          AddFixtureInstancedDraw(fixtureInstancedBatches, obj.mesh, partR,
                                  partG, partB, drawUnlit, wireframe, mode,
                                  RENDER_SCALE, cx, cy, cz, matrix, localMatrix,
                                  worldMatrixArray);
        }
      } else {
        AddFixtureInstancedDraw(fixtureInstancedBatches, FallbackFixtureCubeMesh(),
                                r, g, b, false, wireframe, mode, 0.2f, cx, cy,
                                cz, matrix, nullptr, matrix);
      }
    } else {
      ++frameMetrics.fallbackFixtures;
      frameMetrics.fallbackDrawCalls += drawFixtureGeometry();
    }

    glPopMatrix();

    if (controller.m_captureCanvas && !skipCapture)
      controller.m_captureCanvas->SetSourceKey("unknown");
  }
  if (!fixtureInstancedBatches.empty()) {
    ICanvas2D *prevCanvas = controller.m_captureCanvas;
    bool prevCaptureOnly = controller.m_captureOnly;
    controller.m_captureCanvas = nullptr;
    controller.m_captureOnly = false;
    frameMetrics.instancedDrawCalls =
        RenderFixtureInstancedBatches(fixtureInstancedBatches);
    controller.m_captureCanvas = prevCanvas;
    controller.m_captureOnly = prevCaptureOnly;
  }
  Logger::Instance().Log(
      "fixture render metrics: instancedFixtures=" +
      std::to_string(frameMetrics.instancedFixtures) + ", fallbackFixtures=" +
      std::to_string(frameMetrics.fallbackFixtures) +
      ", instancedDrawCalls=" +
      std::to_string(frameMetrics.instancedDrawCalls) +
      ", fallbackDrawCalls=" +
      std::to_string(frameMetrics.fallbackDrawCalls));
  if (forceFixturesOnTop && depthEnabled)
    glEnable(GL_DEPTH_TEST);
}
