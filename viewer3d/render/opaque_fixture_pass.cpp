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
#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

#include "matrixutils.h"
#include "configmanager.h"
#include "fixture_instanced_renderer.h"
#include "interaction_lod_policy.h"
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

struct FrustumPlane {
  std::array<double, 3> normal = {0.0, 0.0, 0.0};
  double d = 0.0;
};

using FrustumPlanes = std::array<FrustumPlane, 6>;

FrustumPlanes BuildFrustumPlanes(const Viewer3DViewFrustumSnapshot &frustum) {
  std::array<double, 16> clip{};
  const double *m = frustum.model;
  const double *p = frustum.projection;

  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      clip[row * 4 + col] = m[row * 4 + 0] * p[0 * 4 + col] +
                            m[row * 4 + 1] * p[1 * 4 + col] +
                            m[row * 4 + 2] * p[2 * 4 + col] +
                            m[row * 4 + 3] * p[3 * 4 + col];
    }
  }

  auto makePlane = [&](double nx, double ny, double nz, double d) {
    FrustumPlane plane{};
    plane.normal = {nx, ny, nz};
    plane.d = d;
    const double len =
        std::sqrt(plane.normal[0] * plane.normal[0] +
                  plane.normal[1] * plane.normal[1] +
                  plane.normal[2] * plane.normal[2]);
    if (len > 1e-8) {
      plane.normal[0] /= len;
      plane.normal[1] /= len;
      plane.normal[2] /= len;
      plane.d /= len;
    }
    return plane;
  };

  FrustumPlanes planes{};
  planes[0] = makePlane(clip[3] + clip[0], clip[7] + clip[4], clip[11] + clip[8],
                        clip[15] + clip[12]); // left
  planes[1] = makePlane(clip[3] - clip[0], clip[7] - clip[4], clip[11] - clip[8],
                        clip[15] - clip[12]); // right
  planes[2] = makePlane(clip[3] + clip[1], clip[7] + clip[5], clip[11] + clip[9],
                        clip[15] + clip[13]); // bottom
  planes[3] = makePlane(clip[3] - clip[1], clip[7] - clip[5], clip[11] - clip[9],
                        clip[15] - clip[13]); // top
  planes[4] = makePlane(clip[3] + clip[2], clip[7] + clip[6], clip[11] + clip[10],
                        clip[15] + clip[14]); // near
  planes[5] = makePlane(clip[3] - clip[2], clip[7] - clip[6], clip[11] - clip[10],
                        clip[15] - clip[14]); // far
  return planes;
}

bool IsBoundingSphereOutsideFrustum(const Viewer3DBoundingBox &bb,
                                    const FrustumPlanes &planes) {
  const std::array<double, 3> center = {
      (static_cast<double>(bb.min[0]) + static_cast<double>(bb.max[0])) * 0.5,
      (static_cast<double>(bb.min[1]) + static_cast<double>(bb.max[1])) * 0.5,
      (static_cast<double>(bb.min[2]) + static_cast<double>(bb.max[2])) * 0.5};
  const std::array<double, 3> halfExtent = {
      (static_cast<double>(bb.max[0]) - static_cast<double>(bb.min[0])) * 0.5,
      (static_cast<double>(bb.max[1]) - static_cast<double>(bb.min[1])) * 0.5,
      (static_cast<double>(bb.max[2]) - static_cast<double>(bb.min[2])) * 0.5};
  const double radius =
      std::sqrt(halfExtent[0] * halfExtent[0] + halfExtent[1] * halfExtent[1] +
                halfExtent[2] * halfExtent[2]);
  for (const auto &plane : planes) {
    const double signedDistance = plane.normal[0] * center[0] +
                                  plane.normal[1] * center[1] +
                                  plane.normal[2] * center[2] + plane.d;
    if (signedDistance < -radius)
      return true;
  }
  return false;
}

std::array<float, 16> BuildInstancedModelMatrix(const Matrix &worldMatrix) {
  float matrix[16];
  MatrixToArray(worldMatrix, matrix);
  // Positions in MVR/GDTF are authored in millimeters; convert translation
  // to meters so the instanced shader can scale vertices consistently.
  matrix[12] *= RENDER_SCALE;
  matrix[13] *= RENDER_SCALE;
  matrix[14] *= RENDER_SCALE;
  std::array<float, 16> out{};
  std::copy(std::begin(matrix), std::end(matrix), out.begin());
  return out;
}

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
  const Viewer3DViewFrustumSnapshot *frustumSnapshot =
      controller.GetLastFrameFrustumSnapshot();
  std::optional<FrustumPlanes> frustumPlanes;
  if (frustumSnapshot && !context.is2DViewer)
    frustumPlanes = BuildFrustumPlanes(*frustumSnapshot);
  // Keep 2D wireframe fixture overlays unchanged, but in the real 3D wireframe
  // viewer fixtures must respect depth like all other geometry.
  const bool forceFixturesOnTop = wireframe && is2DViewer;
  GLboolean depthEnabled = GL_FALSE;
  if (forceFixturesOnTop) {
    depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled)
      glDisable(GL_DEPTH_TEST);
  }

  const bool canUseHardwareInstancing =
      !context.selectionOverlayPass && !wireframe && !is2DViewer &&
      context.useInteractionProxyLod && !controller.m_captureOnly &&
      controller.m_captureCanvas == nullptr;
  FixtureInstancedBatches instancedBatches;

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
    if (frustumPlanes && fbit != controller.m_fixtureBounds.end() &&
        IsBoundingSphereOutsideFrustum(fbit->second, *frustumPlanes)) {
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
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
    size_t triangleCount = 0;
    if (itg != controller.m_resourceSyncState.loadedGdtf.end()) {
      for (const auto &part : itg->second)
        triangleCount += part.mesh.indices.size() / 3;
    }
    const InteractionLodPolicy interactionLodPolicy{
        context.useInteractionProxyLod, context.interactionProxyMinPixels,
        static_cast<size_t>(std::max(
            0, context.interactionProxyHeavyTriangleThreshold))};
    InteractionLodDecision lodDecision = InteractionLodDecision::None;
    if (!controller.m_captureOnly) {
      lodDecision = EvaluateInteractionLod(
          interactionLodPolicy, frustumSnapshot,
          fbit != controller.m_fixtureBounds.end() ? &fbit->second : nullptr,
          triangleCount);
    }
    if (lodDecision == InteractionLodDecision::Skip) {
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
    }
    const bool useInteractionLodMesh =
        lodDecision == InteractionLodDecision::ProxyBounds &&
        !controller.m_captureOnly;

    if (canUseHardwareInstancing && !highlight && !selected &&
        itg != controller.m_resourceSyncState.loadedGdtf.end()) {
      for (const auto &obj : itg->second) {
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

        const Mesh &meshForPart =
            (useInteractionLodMesh && obj.hasInteractionLodMesh)
                ? obj.interactionLodMesh
                : obj.mesh;
        if (!meshForPart.buffersReady || meshForPart.triangleIndexCount <= 0)
          continue;

        Matrix worldMatrix = MatrixUtils::Multiply(f.transform, obj.transform);
        FixtureInstanceDrawData instanceData;
        instanceData.modelMatrix = BuildInstancedModelMatrix(worldMatrix);
        instanceData.color = {partR, partG, partB};
        instancedBatches[&meshForPart].push_back(std::move(instanceData));
      }

      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
    }

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
    bool placedInstance = false;
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
                  const Mesh &meshForPart =
                      (useInteractionLodMesh && obj.hasInteractionLodMesh)
                          ? obj.interactionLodMesh
                          : obj.mesh;
                  controller.DrawMeshWithOutline(
                      meshForPart, partR, partG, partB, RENDER_SCALE, false, false,
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
        placedInstance = true;
      }
    }

    auto drawFixtureGeometry = [&]() {
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
          const Mesh &meshForPart =
              (useInteractionLodMesh && obj.hasInteractionLodMesh)
                  ? obj.interactionLodMesh
                  : obj.mesh;
          controller.DrawMeshWithOutline(meshForPart, partR, partG, partB,
                                         RENDER_SCALE, highlight, selected, cx,
                                         cy, cz, wireframe, mode, applyCapture,
                                         drawUnlit, partMatrix);
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
      }
    };

    if (renderedPerastageSvg) {
      glPopMatrix();
      if (controller.m_captureCanvas && !skipCapture)
        controller.m_captureCanvas->SetSourceKey("unknown");
      continue;
    }

    if (placedInstance) {
      ICanvas2D *prevCanvas = controller.m_captureCanvas;
      bool prevCaptureOnly = controller.m_captureOnly;
      controller.m_captureCanvas = nullptr;
      controller.m_captureOnly = false;
      drawFixtureGeometry();
      controller.m_captureCanvas = prevCanvas;
      controller.m_captureOnly = prevCaptureOnly;
    } else {
      drawFixtureGeometry();
    }

    glPopMatrix();

    if (controller.m_captureCanvas && !skipCapture)
      controller.m_captureCanvas->SetSourceKey("unknown");
  }
  if (canUseHardwareInstancing && !instancedBatches.empty()) {
    // Dedicated instancing path: draws grouped fixture parts with per-instance
    // transform/color attributes using glDrawElementsInstanced.
    RenderFixtureInstancedBatches(instancedBatches);
  }
  if (forceFixturesOnTop && depthEnabled)
    glEnable(GL_DEPTH_TEST);
}
