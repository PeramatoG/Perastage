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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <unordered_map>
#include <string>
#include <vector>

#include "configmanager.h"
#include "fixture_mesh_color.h"
#include "logger.h"
#include "matrixutils.h"
#include "mesh.h"
#include "opaque_pass_utils.h"
#include "perastage_svg_symbol_builder.h"
#include "pick_mesh_validation.h"
#include "scenedatamanager.h"
#include "symbols/fixture_symbol_availability.h"
#include "symbols/fixture_symbol_preparation_requests.h"
#include "universe_color.h"
#include "viewer3dcontroller.h"
#include <meshoptimizer.h>

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

// Encodes fixture symbol color and projection schema into the style key.
uint32_t BuildFixtureSymbolStyleVersion(float r, float g, float b) {
  auto toByte = [](float value) -> uint32_t {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint32_t>(std::lround(clamped * 255.0f));
  };
  return 0x2000000u | (toByte(r) << 16) | (toByte(g) << 8) | toByte(b);
}

std::vector<SymbolViewKind>
BuildSymbolViewCandidates(SymbolViewKind requested) {
  if (requested == SymbolViewKind::Top)
    return {SymbolViewKind::Top, SymbolViewKind::Bottom};
  if (requested == SymbolViewKind::Bottom)
    return {SymbolViewKind::Bottom, SymbolViewKind::Top};
  if (requested == SymbolViewKind::Front)
    return {SymbolViewKind::Front, SymbolViewKind::Top};
  return {requested};
}

std::array<float, 3> BuildSvgVertexForView(float x, float y, Viewer2DView view);

const Mesh &ResolveMovingProxyMesh(const Mesh &source,
                                   Viewer3DController &controller) {
  const size_t sourceTriangleCount = source.indices.size() / 3;
  // Do not proxy very small meshes; simplification artifacts are more visible
  // than the potential performance gain for tiny triangle counts.
  if (sourceTriangleCount <= 500)
    return source;

  auto computeProxyRatio = [](size_t triangleCount) {
    if (triangleCount <= 2'000)
      return 0.60f;
    if (triangleCount <= 10'000)
      return 0.35f;
    if (triangleCount <= 50'000)
      return 0.20f;
    if (triangleCount <= 200'000)
      return 0.10f;
    return 0.05f;
  };

  static std::unordered_map<const Mesh *, Mesh> proxyCache;
  auto it = proxyCache.find(&source);
  if (it != proxyCache.end())
    return it->second;

  Mesh proxy = source;
  proxy.buffersReady = false;
  proxy.vao = 0;
  proxy.vboVertices = 0;
  proxy.vboNormals = 0;
  proxy.vboTexCoords = 0;
  proxy.vboFlatVertices = 0;
  proxy.vboFlatNormals = 0;
  proxy.eboTriangles = 0;
  proxy.eboLines = 0;

  const size_t vertexCount = proxy.vertices.size() / 3;
  if (vertexCount >= 3 && proxy.indices.size() >= 3) {
    constexpr float kProxySimplifyError = 0.01f;
    constexpr float kProxyOverdrawThreshold = 1.05f;
    constexpr size_t kProxyMinTriangles = 300;
    constexpr size_t kProxyMaxTriangles = 25'000;

    const size_t indexCount = proxy.indices.size();
    const float proxyRatio = computeProxyRatio(sourceTriangleCount);
    const size_t requestedTriangles = static_cast<size_t>(
        static_cast<float>(sourceTriangleCount) * proxyRatio);
    const size_t targetTriangles = std::clamp(
        requestedTriangles, std::min(kProxyMinTriangles, sourceTriangleCount),
        std::min(kProxyMaxTriangles, sourceTriangleCount));
    const size_t targetIndexCount = std::max<size_t>(3, targetTriangles * 3);

    std::vector<uint32_t> simplified(indexCount);
    size_t simplifiedCount =
        meshopt_simplify(simplified.data(), proxy.indices.data(), indexCount,
                         proxy.vertices.data(), vertexCount, sizeof(float) * 3,
                         targetIndexCount, kProxySimplifyError, 0, nullptr);
    const size_t acceptableUpperBound =
        std::max(targetIndexCount + targetIndexCount / 4,
                 std::min(indexCount, indexCount * 4 / 5));

    // Some dense/non-manifold fixtures (often 32-bit/high-poly assets) can be
    // too constrained for a strict low-error pass. Escalate sloppy simplify
    // error in steps before using coarse subsampling to avoid odd artifacts.
    if (simplifiedCount < 3 || simplifiedCount > acceptableUpperBound) {
      const std::vector<float> sloppyErrors = {kProxySimplifyError, 0.02f,
                                               0.05f, 0.10f, 0.20f};
      std::vector<uint32_t> sloppyCandidate(indexCount);
      size_t bestCount = 0;
      std::vector<uint32_t> bestIndices;

      for (float sloppyError : sloppyErrors) {
        size_t candidateCount = meshopt_simplifySloppy(
            sloppyCandidate.data(), proxy.indices.data(), indexCount,
            proxy.vertices.data(), vertexCount, sizeof(float) * 3,
            targetIndexCount, sloppyError, nullptr);
        if (candidateCount < 3)
          continue;

        if (bestCount == 0 || candidateCount < bestCount) {
          bestCount = candidateCount;
          bestIndices.assign(sloppyCandidate.begin(),
                             sloppyCandidate.begin() +
                                 static_cast<std::ptrdiff_t>(candidateCount));
        }

        if (candidateCount <= acceptableUpperBound)
          break;
      }

      if (bestCount >= 3) {
        simplifiedCount = bestCount;
        simplified = std::move(bestIndices);
      }
    }
    if (simplifiedCount < 3 || simplifiedCount >= indexCount * 95 / 100 ||
        simplifiedCount > acceptableUpperBound) {
      // Safety net for very large/complex 32-bit meshes where meshoptimizer
      // cannot reduce enough towards the requested budget: force a coarse
      // triangle subsample so every heavy fixture still gets a lighter proxy
      // while moving the camera.
      const size_t sourceTriangles = indexCount / 3;
      const size_t targetTriangles = std::max<size_t>(1, targetIndexCount / 3);
      simplified.clear();
      simplified.reserve(targetTriangles * 3);
      for (size_t t = 0; t < targetTriangles; ++t) {
        const size_t sourceTri = (t * sourceTriangles) / targetTriangles;
        const size_t base = sourceTri * 3;
        if (base + 2 >= proxy.indices.size())
          break;
        simplified.push_back(proxy.indices[base]);
        simplified.push_back(proxy.indices[base + 1]);
        simplified.push_back(proxy.indices[base + 2]);
      }
      simplifiedCount = simplified.size();
    }
    if (simplifiedCount < 3)
      simplifiedCount = indexCount;
    simplified.resize(simplifiedCount);

    std::vector<uint32_t> cacheOptimized(simplified.size());
    meshopt_optimizeVertexCache(cacheOptimized.data(), simplified.data(),
                                simplified.size(), vertexCount);

    std::vector<uint32_t> overdrawOptimized(cacheOptimized.size());
    meshopt_optimizeOverdraw(overdrawOptimized.data(), cacheOptimized.data(),
                             cacheOptimized.size(), proxy.vertices.data(),
                             vertexCount, sizeof(float) * 3,
                             kProxyOverdrawThreshold);
    proxy.indices = std::move(overdrawOptimized);
  }

  // Upload proxy mesh once so moving-camera rendering stays on the GPU path
  // instead of falling back to immediate-mode CPU submission.
  controller.EnsureMeshGpuBuffers(proxy);

  return proxyCache.emplace(&source, std::move(proxy)).first->second;
}

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
                         GLfloat weight[4], void **outData, void *polygonData) {
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
                          const PerastageSvgSymbolData &svg, Viewer2DView view,
                          double anchorX, double anchorY) {
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
    AppendSvgContourVertices(contourVertices, hole, svg, view, anchorX,
                             anchorY);

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

std::array<float, 3> BuildSvgVertexForView(float x, float y,
                                           Viewer2DView view) {
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
      const auto vertex = BuildSvgVertexForView(
          static_cast<float>(point.x + svg.offsetXmm - anchorX),
          static_cast<float>(point.y + svg.offsetYmm - anchorY), view);
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
      fixtureTransform.u[0],
      fixtureTransform.v[0],
      fixtureTransform.w[0],
      0.0f,
      fixtureTransform.u[1],
      fixtureTransform.v[1],
      fixtureTransform.w[1],
      0.0f,
      fixtureTransform.u[2],
      fixtureTransform.v[2],
      fixtureTransform.w[2],
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
  };
  glMultMatrixf(inverseRotation);
}

// Logs invalid fixture pick mesh data once for each rendered mesh context.
void LogInvalidFixturePickMeshOnce(const Mesh &mesh, const Fixture &fixture,
                                   const std::string &uuid,
                                   const std::string &gdtfPath,
                                   const std::string &meshName) {
  static std::set<std::string> loggedKeys;
  const std::string key = uuid + "|" + gdtfPath + "|" + meshName + "|" +
                          std::to_string(reinterpret_cast<uintptr_t>(&mesh));
  if (!loggedKeys.insert(key).second)
    return;

  const PickMeshValidationStats stats = ValidatePickMeshIndices(mesh);
  Logger::Instance().Log(
      Logger::Level::Warn,
      "Skipping invalid fixture picking triangles: fixture=\"" +
          fixture.instanceName + "\" uuid=" + uuid + " gdtf=\"" +
          gdtfPath + "\" mesh=\"" + meshName +
          "\" invalidTriangles=" +
          std::to_string(stats.invalidTriangleCount) +
          " invalidIndices=" + std::to_string(stats.invalidIndexCount) +
          " vertexCount=" + std::to_string(stats.vertexCount) +
          " indexCount=" + std::to_string(stats.indexCount));
}

// Draws mesh triangles with the current flat OpenGL color for ID picking.
void DrawMeshSolidForPick(const Mesh &mesh, float scale, const Fixture &fixture,
                          const std::string &uuid, const std::string &gdtfPath,
                          const std::string &meshName) {
  bool loggedInvalidData = false;
  glPushMatrix();
  glScalef(scale, scale, scale);
  glBegin(GL_TRIANGLES);
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    if (!IsPickTriangleIndexRangeValid(mesh, i)) {
      if (!loggedInvalidData) {
        glEnd();
        LogInvalidFixturePickMeshOnce(mesh, fixture, uuid, gdtfPath, meshName);
        glBegin(GL_TRIANGLES);
        loggedInvalidData = true;
      }
      continue;
    }
    const uint32_t i0 = mesh.indices[i];
    const uint32_t i1 = mesh.indices[i + 1];
    const uint32_t i2 = mesh.indices[i + 2];
    glVertex3f(mesh.vertices[i0 * 3], mesh.vertices[i0 * 3 + 1],
               mesh.vertices[i0 * 3 + 2]);
    glVertex3f(mesh.vertices[i1 * 3], mesh.vertices[i1 * 3 + 1],
               mesh.vertices[i1 * 3 + 2]);
    glVertex3f(mesh.vertices[i2 * 3], mesh.vertices[i2 * 3 + 1],
               mesh.vertices[i2 * 3 + 2]);
  }
  glEnd();
  glPopMatrix();
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

// Returns true only for fixture metric combinations that have not been logged
// before.
bool ShouldLogFixtureRenderMetrics(const FixtureRenderMetrics &metrics) {
  using MetricsKey = std::array<size_t, 4>;
  static std::set<MetricsKey> loggedMetricKeys;
  const MetricsKey key = {metrics.instancedFixtures, metrics.fallbackFixtures,
                          metrics.instancedDrawCalls,
                          metrics.fallbackDrawCalls};
  return loggedMetricKeys.insert(key).second;
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

// Renders visible fixtures using instanced paths when possible and fallback
// draws otherwise.
void OpaqueFixturePass::Render(
    Viewer3DController &controller, const RenderFrameContext &context,
    const Viewer3DVisibleSet &visibleSet,
    const std::function<std::array<float, 3>(const Fixture &)> &getTypeColor,
    const std::function<std::array<float, 3>(const std::string &)>
        &getLayerColor,
    const std::function<SymbolViewKind(Viewer2DView)> &resolveSymbolView,
    const std::function<std::array<float, 3>(const std::string &)>
        &getPickColor) {
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  if (context.idOnlyPass) {
    glShadeModel(GL_FLAT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    for (const auto &uuid : visibleSet.fixtureUuids) {
      auto fixtureIt = fixtures.find(uuid);
      if (fixtureIt == fixtures.end())
        continue;
      const Fixture &fixture = fixtureIt->second;
      const auto pickColor = getPickColor(uuid);
      glColor3f(pickColor[0], pickColor[1], pickColor[2]);

      float fixtureMatrix[16];
      MatrixToArray(fixture.transform, fixtureMatrix);
      glPushMatrix();
      controller.ApplyTransform(fixtureMatrix, true);

      std::string gdtfPath;
      auto gdtfPathIt = controller.m_resourceSyncState.resolvedGdtfSpecs.find(
          ResolveCacheKey(fixture.gdtfSpec));
      if (gdtfPathIt !=
              controller.m_resourceSyncState.resolvedGdtfSpecs.end() &&
          gdtfPathIt->second.attempted)
        gdtfPath = gdtfPathIt->second.resolvedPath;

      const std::string resourceKey =
          BuildGdtfResourceKey(gdtfPath, fixture.gdtfMode);
      auto gdtfIt = controller.m_resourceSyncState.loadedGdtf.find(resourceKey);
      if (gdtfIt != controller.m_resourceSyncState.loadedGdtf.end()) {
        for (size_t meshIndex = 0; meshIndex < gdtfIt->second.size(); ++meshIndex) {
          const auto &obj = gdtfIt->second[meshIndex];
          float localMatrix[16];
          MatrixToArray(obj.transform, localMatrix);
          glPushMatrix();
          controller.ApplyTransform(localMatrix, false);
          DrawMeshSolidForPick(obj.mesh, RENDER_SCALE, fixture, uuid, gdtfPath,
                               "gdtf-mesh-" + std::to_string(meshIndex));
          glPopMatrix();
        }
      } else {
        DrawMeshSolidForPick(FallbackFixtureCubeMesh(), 0.2f, fixture, uuid,
                             gdtfPath, "fallback-cube");
      }
      glPopMatrix();
    }
    return;
  }
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const bool skipCapture = context.skipCapture;
  const bool is2DViewer = context.is2DViewer;
  const bool isTopView2D = is2DViewer && context.view == Viewer2DView::Top;

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
  const bool drawRealTopInTopView =
      isTopView2D && !forceBottomViewForTopFixtures;

  glShadeModel((context.sketchBasePass ||
                (context.texturedStyle && !wireframe))
                   ? GL_SMOOTH
                   : GL_FLAT);
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
                *key.mesh, r, g, b, key.scale, false, false, false, draw.cx,
                draw.cy, draw.cz, key.wireframe, key.mode,
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

    auto fbit = controller.m_fixtureBounds.find(uuid);

    glPushMatrix();

    std::string fixtureCaptureKey;
    if (controller.m_captureCanvas && !skipCapture) {
      fixtureCaptureKey = !f.typeName.empty()
                              ? f.typeName
                              : (!f.gdtfSpec.empty() ? f.gdtfSpec : "unknown");
      controller.m_captureCanvas->SetSourceKey(fixtureCaptureKey);
    }

    const bool highlight = !context.idOnlyPass &&
                           !controller.m_highlightUuid.empty() &&
                           uuid == controller.m_highlightUuid;
    const bool selected =
        !context.idOnlyPass && controller.m_primarySelectedUuids.find(uuid) !=
                                   controller.m_primarySelectedUuids.end();
    const bool groupHighlight =
        !context.idOnlyPass &&
        (controller.m_groupHighlightUuids.find(uuid) !=
             controller.m_groupHighlightUuids.end() ||
         (controller.m_selectedUuids.find(uuid) != controller.m_selectedUuids.end() &&
          !selected));

    float matrix[16];
    MatrixToArray(f.transform, matrix);
    controller.ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    fbit = controller.m_fixtureBounds.find(uuid);
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

    float r = viewer3d::render::DEFAULT_MESH_COLOR[0];
    float g = viewer3d::render::DEFAULT_MESH_COLOR[1];
    float b = viewer3d::render::DEFAULT_MESH_COLOR[2];
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
      auto c = getTypeColor(f);
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

    auto applyFixtureCapture =
        [fixtureTransform](const std::array<float, 3> &p) {
          return TransformPoint(fixtureTransform, p);
        };

    const std::string normalizedGdtfPath = NormalizeModelKeyPath(gdtfPath);
    const std::string modelKey =
        BuildFixtureSymbolModelKey(f, normalizedGdtfPath);
    const std::string svgSourcePath = normalizedGdtfPath;

    const std::string resourceKey =
        BuildGdtfResourceKey(gdtfPath, f.gdtfMode);
    auto itg = controller.m_resourceSyncState.loadedGdtf.find(resourceKey);

    bool renderedPerastageSvg = false;
    const bool captureRecordingActive =
        controller.m_captureCanvas && !skipCapture;
    const bool preferLayoutSvg = false;
    (void)captureRecordingActive;
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
          if (const auto svg = symbol_cache::LoadUsableFixtureSymbol(
                  svgSourcePath, candidateView))
            loaded = *svg;
          cacheIt =
              perastageSvgCache.emplace(cacheKey, std::move(loaded)).first;
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
         mode != Viewer2DRenderMode::Wireframe && !highlight &&
         !groupHighlight && !selected);
    bool placedSymbolInstance = false;
    if (useSymbolInstancing && controller.m_captureCanvas && !skipCapture) {
      if (!modelKey.empty()) {
        const Viewer2DView fixtureCaptureView =
            isTopView2D && forceBottomViewForTopFixtures
                ? Viewer2DView::Bottom
                : controller.m_captureView;

        SymbolKey symbolKey;
        symbolKey.modelKey = modelKey;
        const FixtureSymbolProjection symbolProjection =
            ResolveFixtureSymbolProjection(fixtureTransform, fixtureCaptureView,
                                           forceBottomViewForTopFixtures);
        if (!symbolProjection.valid)
          continue;

        symbolKey.viewKind = symbolProjection.symbolView;
        symbolKey.styleVersion = BuildFixtureSymbolStyleVersion(r, g, b);

        const auto &symbol = controller.m_bottomSymbolCache.GetOrCreate(
            symbolKey, [&](const SymbolKey &, uint32_t symbolId) {
              SymbolDefinition svgDefinition{};
              if (!svgSourcePath.empty() &&
                  TryBuildPerastageSvgSymbolDefinition(
                      svgSourcePath, symbolKey.viewKind, symbolId, {r, g, b},
                      svgDefinition)) {
                return svgDefinition;
              }
              if (!svgSourcePath.empty())
                symbols::RequestFixtureSymbolPreparation(svgSourcePath,
                                                         f.gdtfMode);

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
                      false, 0.0f, 0.0f, 0.0f, wireframe, mode, applyCapture,
                      false);
                }
              } else {
                controller.m_captureCanvas->SetSourceKey(fixtureCaptureKey);
                const bool fallbackWireframe = wireframe;
                const bool fallbackUnlit =
                    context.whiteModelStyle &&
                    !controller.IsSketchRenderStyleEnabled();
                controller.DrawMeshWithOutline(
                    FallbackFixtureCubeMesh(), r, g, b, 0.2f, false, false,
                    false, 0.0f, 0.0f, 0.0f, fallbackWireframe, mode,
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

        controller.m_captureCanvas->PlaceSymbolInstance(
            symbol.symbolId, symbolProjection.instanceTransform);
        placedSymbolInstance = true;
      }
    }

    if (placedSymbolInstance) {
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
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
          Matrix worldMatrix =
              MatrixUtils::Multiply(f.transform, obj.transform);
          float partMatrix[16];
          MatrixToArray(worldMatrix, partMatrix);
          auto applyCapture = [fixtureTransform, objTransform = obj.transform](
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
          controller.DrawMeshWithOutline(
              obj.mesh, partR, partG, partB, RENDER_SCALE, highlight,
              groupHighlight, selected, cx, cy, cz, wireframe, mode,
              applyCapture, drawUnlit, partMatrix);
          ++drawCalls;
          glPopMatrix();
        }
      } else {
        const bool fallbackWireframe = wireframe;
        const bool fallbackUnlit = !highlight && !groupHighlight && !selected &&
                                   context.whiteModelStyle &&
                                   !controller.IsSketchRenderStyleEnabled();
        controller.DrawMeshWithOutline(
            FallbackFixtureCubeMesh(), r, g, b, 0.2f, highlight, groupHighlight,
            selected, cx, cy, cz, fallbackWireframe, mode, applyFixtureCapture,
            fallbackUnlit, matrix);
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

    // Uses proxy/instanced fixture draws only for non-highlighted fast
    // interaction frames.
    if (context.skipOptionalWork && !highlight && !groupHighlight &&
        !selected && !captureRecordingActive) {
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
          Matrix worldMatrix =
              MatrixUtils::Multiply(f.transform, obj.transform);
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
          const Mesh &proxyMesh = ResolveMovingProxyMesh(obj.mesh, controller);
          AddFixtureInstancedDraw(fixtureInstancedBatches, proxyMesh, partR,
                                  partG, partB, drawUnlit, wireframe, mode,
                                  RENDER_SCALE, cx, cy, cz, matrix, localMatrix,
                                  worldMatrixArray);
        }
      } else {
        AddFixtureInstancedDraw(
            fixtureInstancedBatches, FallbackFixtureCubeMesh(), r, g, b, false,
            wireframe, mode, 0.2f, cx, cy, cz, matrix, nullptr, matrix);
      }
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
    }

    // Proxy/instanced fixture batching is reserved for the fast-interaction
    // path while the camera is moving. When the camera is static we draw full
    // geometry directly to avoid persistent proxy rendering.
    const bool eligibleForFixtureInstancedBatch =
        context.skipOptionalWork && !highlight && !groupHighlight &&
        !selected && !captureRecordingActive;
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
          Matrix worldMatrix =
              MatrixUtils::Multiply(f.transform, obj.transform);
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
        AddFixtureInstancedDraw(
            fixtureInstancedBatches, FallbackFixtureCubeMesh(), r, g, b, false,
            wireframe, mode, 0.2f, cx, cy, cz, matrix, nullptr, matrix);
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
  if (ShouldLogFixtureRenderMetrics(frameMetrics)) {
    Logger::Instance().Log(
        "fixture render metrics: instancedFixtures=" +
        std::to_string(frameMetrics.instancedFixtures) +
        ", fallbackFixtures=" + std::to_string(frameMetrics.fallbackFixtures) +
        ", instancedDrawCalls=" +
        std::to_string(frameMetrics.instancedDrawCalls) +
        ", fallbackDrawCalls=" +
        std::to_string(frameMetrics.fallbackDrawCalls));
  }
  if (forceFixturesOnTop && depthEnabled)
    glEnable(GL_DEPTH_TEST);
}
