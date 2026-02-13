/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
/*
 * File: viewer3dcontroller.cpp
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Implementation of 3D viewer logic.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText // Avoid conflict with Windows GDI macro of the same name
#endif

#include <GL/glew.h>
// macOS provides OpenGL via the framework, so choose headers by platform.
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <cstdlib>
#include <numeric>

#include "configmanager.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "scenedatamanager.h"
#include "matrixutils.h"
#include "viewer3dcontroller.h"
// Include shared Matrix type used throughout models
#include "types.h"
#include "consolepanel.h"
#include "logger.h"
#include "projectutils.h"
#include "scenerenderer.h"
#include "render_pipeline.h"
#include "opaque_fixture_pass.h"
#include "opaque_object_pass.h"
#include "opaque_truss_pass.h"
#include "bounds_cache_system.h"
#include "visibilitysystem.h"
#include "label_render_system.h"
#include "selectionsystem.h"
#include "gl_primitive_renderer.h"

#include <wx/wx.h>
#define NANOVG_GL2_IMPLEMENTATION
#include <algorithm>
#include <bit>
#include <array>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cstdint>
#include <string_view>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

struct Viewer3DController::Impl {
  ResourceSyncState resourceSyncState;
  std::unordered_map<std::string, BoundingBox> modelBounds;
  size_t sceneVersion = 0;
  size_t cachedVersion = static_cast<size_t>(-1);
  bool sceneChangedDirty = true;
  bool assetsChangedDirty = true;
  bool visibilityChangedDirty = true;
  std::unordered_map<std::string, BoundingBox> fixtureBounds;
  std::unordered_map<std::string, BoundingBox> trussBounds;
  std::unordered_map<std::string, BoundingBox> objectBounds;
  std::unordered_set<std::string> boundsHiddenLayers;
  std::vector<const std::pair<const std::string, Fixture> *> sortedFixtures;
  std::vector<const std::pair<const std::string, Truss> *> sortedTrusses;
  std::vector<const std::pair<const std::string, SceneObject> *> sortedObjects;
  std::vector<const std::pair<const std::string, Fixture> *> visibleSortedFixtures;
  std::vector<const std::pair<const std::string, Truss> *> visibleSortedTrusses;
  std::vector<const std::pair<const std::string, SceneObject> *> visibleSortedObjects;
  std::unordered_set<std::string> lastHiddenLayers;
  size_t hiddenLayersVersion = 0;
  bool sortedListsDirty = true;
  mutable std::mutex sortedListsMutex;
  std::unordered_map<std::string, std::array<float, 3>> typeColors;
  std::unordered_map<std::string, std::array<float, 3>> layerColors;
  std::string highlightUuid;
  std::unordered_set<std::string> selectedUuids;
  NVGcontext *vg = nullptr;
  int font = -1;
  int fontBold = -1;
  ICanvas2D *captureCanvas = nullptr;
  Viewer2DView captureView = Viewer2DView::Top;
  bool captureIncludeGrid = true;
  bool captureOnly = false;
  bool captureUseSymbols = false;
  SymbolCache bottomSymbolCache;
  bool darkMode = false;
  bool showSelectionOutline2D = false;
  bool isInteracting = false;
  bool cameraMoving = false;
  bool useAdaptiveLineProfile = true;
  bool skipOutlinesForCurrentFrame = false;
  int updateResourcesCallsPerFrame = 0;
  mutable VisibleSet cachedVisibleSet;
  mutable VisibleSet cachedLayerVisibleCandidates;
  mutable size_t layerVisibleCandidatesSceneVersion = static_cast<size_t>(-1);
  mutable std::unordered_set<std::string> layerVisibleCandidatesHiddenLayers;
  mutable size_t layerVisibleCandidatesRevision = 0;
  mutable size_t visibleSetLayerCandidatesRevision = static_cast<size_t>(-1);
  mutable bool visibleSetFrustumCulling = false;
  mutable float visibleSetMinPixels = -1.0f;
  mutable std::array<int, 4> visibleSetViewport = {0, 0, 0, 0};
  mutable std::array<double, 16> visibleSetModel = {};
  mutable std::array<double, 16> visibleSetProjection = {};
  std::unique_ptr<SceneRenderer> sceneRenderer;
  std::unique_ptr<VisibilitySystem> visibilitySystem;
  std::unique_ptr<SelectionSystem> selectionSystem;
  std::unique_ptr<LabelRenderSystem> labelRenderSystem;
};

struct LineRenderProfile {
  float lineWidth = 1.0f;
  bool enableLineSmoothing = false;
};

static std::unordered_set<std::string>
SnapshotHiddenLayers(const ConfigManager &cfg) {
  return cfg.GetHiddenLayers();
}

static bool IsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                                 const std::string &layer) {
  if (layer.empty())
    return hidden.find(DEFAULT_LAYER_NAME) == hidden.end();
  return hidden.find(layer) == hidden.end();
}

static bool IsFastInteractionModeEnabled(const ConfigManager &cfg) {
  return cfg.GetFloat("viewer3d_fast_interaction_mode") >= 0.5f;
}

static LineRenderProfile GetLineRenderProfile(bool isInteracting,
                                              bool wireframeMode,
                                              bool adaptiveEnabled) {
  if (!adaptiveEnabled)
    return {wireframeMode ? 1.0f : 2.0f, false};
  if (isInteracting)
    return {1.0f, false};
  return {wireframeMode ? 1.0f : 2.0f, true};
}

// Replace Windows path separators with the platform preferred one
static std::string NormalizePath(const std::string &p) {
  std::string out = p;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  return out;
}

static std::string NormalizeModelKey(const std::string &p) {
  if (p.empty())
    return {};
  fs::path path(p);
  path = path.lexically_normal();
  return NormalizePath(path.string());
}

struct EdgeKey {
  unsigned short a = 0;
  unsigned short b = 0;

  bool operator==(const EdgeKey &other) const {
    return a == other.a && b == other.b;
  }
};

struct EdgeKeyHash {
  size_t operator()(const EdgeKey &key) const {
    return (static_cast<size_t>(key.a) << 16u) ^ static_cast<size_t>(key.b);
  }
};

struct EdgeInfo {
  int count = 0;
  std::array<float, 3> firstFaceNormal = {0.0f, 0.0f, 0.0f};
  std::array<float, 3> secondFaceNormal = {0.0f, 0.0f, 0.0f};
};

static std::array<float, 3> BuildFaceNormal(const std::vector<float> &vertices,
                                            unsigned short i0,
                                            unsigned short i1,
                                            unsigned short i2) {
  const size_t p0 = static_cast<size_t>(i0) * 3u;
  const size_t p1 = static_cast<size_t>(i1) * 3u;
  const size_t p2 = static_cast<size_t>(i2) * 3u;
  if (p0 + 2 >= vertices.size() || p1 + 2 >= vertices.size() ||
      p2 + 2 >= vertices.size())
    return {0.0f, 0.0f, 0.0f};

  const float ax = vertices[p1] - vertices[p0];
  const float ay = vertices[p1 + 1] - vertices[p0 + 1];
  const float az = vertices[p1 + 2] - vertices[p0 + 2];
  const float bx = vertices[p2] - vertices[p0];
  const float by = vertices[p2 + 1] - vertices[p0 + 1];
  const float bz = vertices[p2 + 2] - vertices[p0 + 2];

  float nx = ay * bz - az * by;
  float ny = az * bx - ax * bz;
  float nz = ax * by - ay * bx;
  const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
  if (len <= 1e-6f)
    return {0.0f, 0.0f, 0.0f};

  nx /= len;
  ny /= len;
  nz /= len;
  return {nx, ny, nz};
}

static std::vector<unsigned short>
BuildWireframeIndices(const std::vector<float> &vertices,
                      const std::vector<unsigned short> &triangleIndices) {
  static constexpr float kCreaseAngleDeg = 5.0f;
  static constexpr float kPi = 3.14159265358979323846f;
  const float creaseDotThreshold =
      std::cos(kCreaseAngleDeg * kPi / 180.0f);

  std::unordered_map<EdgeKey, EdgeInfo, EdgeKeyHash> edges;
  edges.reserve(triangleIndices.size());

  auto registerEdge = [&](unsigned short i, unsigned short j,
                          const std::array<float, 3> &faceNormal) {
    const EdgeKey key = {std::min(i, j), std::max(i, j)};
    EdgeInfo &info = edges[key];
    ++info.count;
    if (info.count == 1)
      info.firstFaceNormal = faceNormal;
    else if (info.count == 2)
      info.secondFaceNormal = faceNormal;
  };

  for (size_t i = 0; i + 2 < triangleIndices.size(); i += 3) {
    const unsigned short i0 = triangleIndices[i];
    const unsigned short i1 = triangleIndices[i + 1];
    const unsigned short i2 = triangleIndices[i + 2];
    const std::array<float, 3> faceNormal = BuildFaceNormal(vertices, i0, i1, i2);

    registerEdge(i0, i1, faceNormal);
    registerEdge(i1, i2, faceNormal);
    registerEdge(i2, i0, faceNormal);
  }

  std::vector<unsigned short> lineIndices;
  lineIndices.reserve(edges.size() * 2u);
  for (const auto &[edge, info] : edges) {
    if (info.count == 1) {
      lineIndices.push_back(edge.a);
      lineIndices.push_back(edge.b);
      continue;
    }

    if (info.count == 2) {
      const float dot = info.firstFaceNormal[0] * info.secondFaceNormal[0] +
                        info.firstFaceNormal[1] * info.secondFaceNormal[1] +
                        info.firstFaceNormal[2] * info.secondFaceNormal[2];
      if (dot < creaseDotThreshold) {
        lineIndices.push_back(edge.a);
        lineIndices.push_back(edge.b);
      }
    }
  }
  return lineIndices;
}

static uint32_t HashString(std::string_view value) {
  uint32_t hash = 2166136261u;
  for (unsigned char c : value) {
    hash ^= c;
    hash *= 16777619u;
  }
  return hash;
}

static std::array<float, 3> HsvToRgb(float h, float s, float v) {
  const float c = v * s;
  const float hh = h * 6.0f;
  const float x = c * (1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f));
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;

  if (hh >= 0.0f && hh < 1.0f) {
    r = c;
    g = x;
  } else if (hh >= 1.0f && hh < 2.0f) {
    r = x;
    g = c;
  } else if (hh >= 2.0f && hh < 3.0f) {
    g = c;
    b = x;
  } else if (hh >= 3.0f && hh < 4.0f) {
    g = x;
    b = c;
  } else if (hh >= 4.0f && hh < 5.0f) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }

  const float m = v - c;
  return {r + m, g + m, b + m};
}

static std::array<float, 3> MakeDeterministicColor(std::string_view key) {
  if (key.empty())
    key = "default";
  uint32_t hash = HashString(key);
  float hue = static_cast<float>(hash % 360u) / 360.0f;
  float sat = 0.55f + static_cast<float>((hash >> 8) & 0xFFu) / 255.0f * 0.35f;
  float val = 0.7f + static_cast<float>((hash >> 16) & 0xFFu) / 255.0f * 0.25f;
  return HsvToRgb(hue, sat, val);
}


static SymbolBounds ComputeSymbolBounds(const CommandBuffer &buffer) {
  SymbolBounds bounds{};
  bool hasPoint = false;

  auto addPoint = [&](float x, float y) {
    if (!hasPoint) {
      bounds.min = {x, y};
      bounds.max = {x, y};
      hasPoint = true;
      return;
    }
    bounds.min.x = std::min(bounds.min.x, x);
    bounds.min.y = std::min(bounds.min.y, y);
    bounds.max.x = std::max(bounds.max.x, x);
    bounds.max.y = std::max(bounds.max.y, y);
  };

  auto addPointWithPadding = [&](float x, float y, float padding) {
    if (padding <= 0.0f) {
      addPoint(x, y);
      return;
    }
    addPoint(x - padding, y - padding);
    addPoint(x + padding, y + padding);
  };

  auto addPoints = [&](const std::vector<float> &points, float padding) {
    for (size_t i = 0; i + 1 < points.size(); i += 2)
      addPointWithPadding(points[i], points[i + 1], padding);
  };

  for (const auto &cmd : buffer.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      float padding = line->stroke.width * 0.5f;
      addPointWithPadding(line->x0, line->y0, padding);
      addPointWithPadding(line->x1, line->y1, padding);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      float padding = polyline->stroke.width * 0.5f;
      addPoints(polyline->points, padding);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      float padding = poly->stroke.width * 0.5f;
      addPoints(poly->points, padding);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      float padding = rect->stroke.width * 0.5f;
      addPoint(rect->x - padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y + rect->h + padding);
      addPoint(rect->x - padding, rect->y + rect->h + padding);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      float padding = circle->stroke.width * 0.5f;
      float radius = circle->radius + padding;
      addPoint(circle->cx - radius, circle->cy - radius);
      addPoint(circle->cx + radius, circle->cy + radius);
    }
  }

  if (!hasPoint) {
    bounds.min = {};
    bounds.max = {};
  }
  return bounds;
}

static bool HexToRGB(const std::string &hex, float &r, float &g, float &b) {
  if (hex.size() != 7 || hex[0] != '#')
    return false;
  unsigned int value = 0;
  std::istringstream iss(hex.substr(1));
  iss >> std::hex >> value;
  if (iss.fail())
    return false;
  r = ((value >> 16) & 0xFF) / 255.0f;
  g = ((value >> 8) & 0xFF) / 255.0f;
  b = (value & 0xFF) / 255.0f;
  return true;
}

static void MatrixToArray(const Matrix &m, float out[16]) {
  out[0] = m.u[0];
  out[1] = m.u[1];
  out[2] = m.u[2];
  out[3] = 0.0f;
  out[4] = m.v[0];
  out[5] = m.v[1];
  out[6] = m.v[2];
  out[7] = 0.0f;
  out[8] = m.w[0];
  out[9] = m.w[1];
  out[10] = m.w[2];
  out[11] = 0.0f;
  out[12] = m.o[0];
  out[13] = m.o[1];
  out[14] = m.o[2];
  out[15] = 1.0f;
}

struct CullingSettings {
  bool enabled = true;
  float minPixels3D = 2.0f;
  float minPixels2D = 1.0f;
};

static CullingSettings GetCullingSettings3D(const ConfigManager &cfg) {
  CullingSettings s{};
  s.enabled = cfg.GetFloat("render_culling_enabled") >= 0.5f;
  s.minPixels3D = std::max(0.0f, cfg.GetFloat("render_culling_min_pixels_3d"));
  s.minPixels2D = std::max(0.0f, cfg.GetFloat("render_culling_min_pixels_2d"));
  return s;
}

static std::array<float, 3> TransformPoint(const Matrix &m,
                                           const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}


static size_t HashCombine(size_t seed, size_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

static size_t HashFloat(float value) {
  return std::hash<uint32_t>{}(std::bit_cast<uint32_t>(value));
}

static size_t HashString(const std::string &value) {
  return std::hash<std::string>{}(value);
}

static size_t HashMatrix(const Matrix &m) {
  size_t seed = 0;
  const std::array<float, 12> vals = {m.u[0], m.u[1], m.u[2], m.v[0], m.v[1],
                                      m.v[2], m.w[0], m.w[1], m.w[2], m.o[0],
                                      m.o[1], m.o[2]};
  for (float v : vals)
    seed = HashCombine(seed, HashFloat(v));
  return seed;
}


static Transform2D BuildInstanceTransform2D(const Matrix &m, Viewer2DView view) {
  Transform2D t{};
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    t.a = m.u[0];
    t.b = m.u[1];
    t.c = m.v[0];
    t.d = m.v[1];
    t.tx = m.o[0];
    t.ty = m.o[1];
    break;
  case Viewer2DView::Front:
    t.a = m.u[0];
    t.b = m.u[2];
    t.c = m.w[0];
    t.d = m.w[2];
    t.tx = m.o[0];
    t.ty = m.o[2];
    break;
  case Viewer2DView::Side:
    t.a = -m.v[1];
    t.b = m.v[2];
    t.c = -m.w[1];
    t.d = m.w[2];
    t.tx = -m.o[1];
    t.ty = m.o[2];
    break;
  }
  return t;
}

// Maps a 3D point expressed in meters to the 2D canvas coordinates used by the
// simplified viewer. The mapping mirrors the orthographic camera setup in
// Render() so exporters can rebuild the same projection.
std::array<float, 2>
Viewer3DController::ProjectToCanvas(const std::array<float, 3> &p) const {
  switch (m_impl->captureView) {
  case Viewer2DView::Top:
    return {p[0], p[1]};
  case Viewer2DView::Bottom:
    return {p[0], p[1]};
  case Viewer2DView::Front:
    return {p[0], p[2]};
  case Viewer2DView::Side:
    // The side camera looks towards +X with +Z as the up vector, which makes
    // the screen X axis point towards -Y in world space. Mirror the Y
    // coordinate accordingly so the recorded commands match the on-screen
    // orientation used by the 2D viewer.
    return {-p[1], p[2]};
  }
  return {p[0], p[1]};
}

void Viewer3DController::RecordLine(const std::array<float, 3> &a,
                                    const std::array<float, 3> &b,
                                    const CanvasStroke &stroke) const {
  if (!m_impl->captureCanvas)
    return;
  auto p0 = ProjectToCanvas(a);
  auto p1 = ProjectToCanvas(b);
  m_impl->captureCanvas->DrawLine(p0[0], p0[1], p1[0], p1[1], stroke);
}

void Viewer3DController::RecordPolyline(
    const std::vector<std::array<float, 3>> &points,
    const CanvasStroke &stroke) const {
  if (!m_impl->captureCanvas || points.size() < 2)
    return;
  std::vector<float> flat;
  flat.reserve(points.size() * 2);
  for (const auto &p : points) {
    auto q = ProjectToCanvas(p);
    flat.push_back(q[0]);
    flat.push_back(q[1]);
  }
  m_impl->captureCanvas->DrawPolyline(flat, stroke);
}

void Viewer3DController::RecordPolygon(
    const std::vector<std::array<float, 3>> &points,
    const CanvasStroke &stroke, const CanvasFill *fill) const {
  if (!m_impl->captureCanvas || points.size() < 3)
    return;
  std::vector<float> flat;
  flat.reserve(points.size() * 2);
  for (const auto &p : points) {
    auto q = ProjectToCanvas(p);
    flat.push_back(q[0]);
    flat.push_back(q[1]);
  }
  m_impl->captureCanvas->DrawPolygon(flat, stroke, fill);
}

void Viewer3DController::RecordText(float x, float y, const std::string &text,
                                    const CanvasTextStyle &style) const {
  if (!m_impl->captureCanvas)
    return;
  m_impl->captureCanvas->DrawText(x, y, text, style);
}

Viewer3DController::Viewer3DController()
    : m_impl(std::make_unique<Impl>()) {
  m_impl->sceneRenderer = std::make_unique<SceneRenderer>(*this);
  m_impl->visibilitySystem = std::make_unique<VisibilitySystem>(*this);
  m_impl->selectionSystem = std::make_unique<SelectionSystem>(*this);
  m_impl->labelRenderSystem = std::make_unique<LabelRenderSystem>(*this);
  // Actual initialization of OpenGL-dependent resources is delayed
  // until a valid context is available.
}

Viewer3DController::~Viewer3DController() {
  for (auto &[path, mesh] : m_impl->resourceSyncState.loadedMeshes) {
    (void)path;
    ReleaseMeshBuffers(mesh);
  }
  if (m_impl->vg)
    nvgDeleteGL2(m_impl->vg);
}

void Viewer3DController::InitializeGL() {
  if (m_impl->vg)
    return; // Already initialized

  m_impl->vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  if (m_impl->vg) {
    // Windows uses Arial (C:/Windows/Fonts/arial.ttf); Linux uses
    // DejaVuSans (/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf) for NanoVG
    // labels.
    std::vector<fs::path> fontPaths = {
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf",
#endif
#ifdef __APPLE__
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
#endif
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
    std::vector<fs::path> boldFontPaths = {
#ifdef _WIN32
        "C:/Windows/Fonts/arialbd.ttf",
#endif
#ifdef __APPLE__
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/Library/Fonts/Arial Bold.ttf",
#endif
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"};

    const fs::path resourceRoot = ProjectUtils::GetResourceRoot();
    std::vector<fs::path> bundledFontPaths;
    std::vector<fs::path> bundledBoldFontPaths;
    if (!resourceRoot.empty()) {
      bundledFontPaths.emplace_back(resourceRoot / "fonts" /
                                    "PerastageSans.ttf");
      bundledBoldFontPaths.emplace_back(resourceRoot / "fonts" /
                                        "PerastageSans-Bold.ttf");
    }

    auto loadFontFromPaths = [&](const std::vector<fs::path> &paths,
                                 const char *name, int &target) -> bool {
      std::error_code ec;
      for (const auto &path : paths) {
        if (path.empty())
          continue;
        if (fs::exists(path, ec)) {
          std::string pathString = path.string();
          target = nvgCreateFont(m_impl->vg, name, pathString.c_str());
          if (target >= 0)
            return true;
        }
        ec.clear();
      }
      return false;
    };

    loadFontFromPaths(fontPaths, "sans", m_impl->font);
    loadFontFromPaths(boldFontPaths, "sans-bold", m_impl->fontBold);

#ifdef __APPLE__
    if (m_impl->font < 0 && !bundledFontPaths.empty()) {
      Logger::Instance().Log(
          "Failed to load system font for labels; trying bundled font");
    }
    if (m_impl->fontBold < 0 && !bundledBoldFontPaths.empty()) {
      Logger::Instance().Log(
          "Failed to load system bold font for labels; trying bundled font");
    }
#endif

    if (m_impl->font < 0)
      loadFontFromPaths(bundledFontPaths, "sans", m_impl->font);
    if (m_impl->fontBold < 0)
      loadFontFromPaths(bundledBoldFontPaths, "sans-bold", m_impl->fontBold);
    if (m_impl->fontBold < 0)
      m_impl->fontBold = m_impl->font;
    if (m_impl->font < 0)
      Logger::Instance().Log("Failed to load font for labels");
    if (m_impl->fontBold < 0)
      Logger::Instance().Log("Failed to load bold font for labels");
  } else {
    Logger::Instance().Log("Failed to create NanoVG context");
  }
}

void Viewer3DController::SetHighlightUuid(const std::string &uuid) {
  m_impl->selectionSystem->SetHighlightUuid(uuid);
}

void Viewer3DController::SetSelectedUuids(
    const std::vector<std::string> &uuids) {
  m_impl->selectionSystem->SetSelectedUuids(uuids);
}

void Viewer3DController::ApplyHighlightUuid(const std::string &uuid) {
  m_impl->highlightUuid = uuid;
}

void Viewer3DController::ReplaceSelectedUuids(
    const std::vector<std::string> &uuids) {
  m_impl->selectedUuids.clear();
  for (const auto &u : uuids)
    m_impl->selectedUuids.insert(u);
}

const Viewer3DController::BoundingBox *
Viewer3DController::FindFixtureBounds(const std::string &uuid) const {
  auto it = m_impl->fixtureBounds.find(uuid);
  return it == m_impl->fixtureBounds.end() ? nullptr : &it->second;
}

const Viewer3DController::BoundingBox *
Viewer3DController::FindTrussBounds(const std::string &uuid) const {
  auto it = m_impl->trussBounds.find(uuid);
  return it == m_impl->trussBounds.end() ? nullptr : &it->second;
}

const Viewer3DController::BoundingBox *
Viewer3DController::FindObjectBounds(const std::string &uuid) const {
  auto it = m_impl->objectBounds.find(uuid);
  return it == m_impl->objectBounds.end() ? nullptr : &it->second;
}


bool Viewer3DController::EnsureBoundsComputed(
    const std::string &uuid, ItemType type,
    const std::unordered_set<std::string> &hiddenLayers) {
  return m_impl->visibilitySystem->EnsureBoundsComputed(uuid, type, hiddenLayers);
}

// Loads meshes or GDTF models referenced by scene objects. Called when the
// scene is updated.
void Viewer3DController::Update() {
  UpdateFrameStateLightweight();
}

void Viewer3DController::UpdateFrameStateLightweight() {
  ConfigManager &cfg = ConfigManager::Get();
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  if (hiddenLayers != m_impl->lastHiddenLayers) {
    Logger::Instance().Log("visibility dirty reason: hidden layers changed vs last frame snapshot");
    m_impl->visibilityChangedDirty = true;
  }
}

void Viewer3DController::ResetDebugPerFrameCounters() {
  m_impl->updateResourcesCallsPerFrame = 0;
}

int Viewer3DController::GetDebugUpdateResourcesCallsPerFrame() const {
  return m_impl->updateResourcesCallsPerFrame;
}

void Viewer3DController::UpdateResourcesIfDirty() {
  ++m_impl->updateResourcesCallsPerFrame;

  ConfigManager &cfg = ConfigManager::Get();
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const std::string &base = cfg.GetScene().basePath;

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  const auto &objects = SceneDataManager::Instance().GetSceneObjects();
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  std::vector<const std::pair<const std::string, Truss> *> visibleTrusses;
  std::vector<const std::pair<const std::string, SceneObject> *> visibleObjects;
  std::vector<const std::pair<const std::string, Fixture> *> visibleFixtures;
  visibleTrusses.reserve(trusses.size());
  visibleObjects.reserve(objects.size());
  visibleFixtures.reserve(fixtures.size());
  for (const auto &entry : trusses) {
    if (IsLayerVisibleCached(hiddenLayers, entry.second.layer))
      visibleTrusses.push_back(&entry);
  }
  for (const auto &entry : objects) {
    if (IsLayerVisibleCached(hiddenLayers, entry.second.layer))
      visibleObjects.push_back(&entry);
  }
  for (const auto &entry : fixtures) {
    if (IsLayerVisibleCached(hiddenLayers, entry.second.layer))
      visibleFixtures.push_back(&entry);
  }

  ResourceSyncCallbacks callbacks;
  callbacks.setupMeshBuffers = [this](Mesh &mesh) { SetupMeshBuffers(mesh); };
  callbacks.releaseMeshBuffers = [this](Mesh &mesh) { ReleaseMeshBuffers(mesh); };
  callbacks.appendConsoleMessage = [](const std::string &msg) {
    if (ConsolePanel::Instance())
      ConsolePanel::Instance()->AppendMessage(wxString::FromUTF8(msg));
  };

  const ResourceSyncResult syncResult = ResourceSyncSystem::Sync(
      base, visibleTrusses, visibleObjects, visibleFixtures,
      m_impl->resourceSyncState, callbacks);

  if (syncResult.sceneChanged) {
    ++m_impl->sceneVersion;
    m_impl->sceneChangedDirty = true;
    std::lock_guard<std::mutex> lock(m_impl->sortedListsMutex);
    m_impl->sortedListsDirty = true;
  }
  if (syncResult.assetsChanged) {
    m_impl->assetsChangedDirty = true;
    m_impl->modelBounds.clear();
  }

  BoundsCacheSystem::Context boundsContext{
      m_impl->resourceSyncState, m_impl->modelBounds, m_impl->fixtureBounds,
      m_impl->trussBounds,      m_impl->objectBounds, m_impl->boundsHiddenLayers,
      m_impl->sceneVersion,     m_impl->cachedVersion, m_impl->sceneChangedDirty,
      m_impl->assetsChangedDirty, m_impl->visibilityChangedDirty,
      m_impl->sortedListsMutex, m_impl->sortedListsDirty};
  BoundsCacheSystem::RebuildIfDirty(boundsContext, hiddenLayers, trusses,
                                    objects, fixtures);
  m_impl->lastHiddenLayers = hiddenLayers;
}



bool Viewer3DController::TryBuildLayerVisibleCandidates(
    const std::unordered_set<std::string> &hiddenLayers,
    VisibleSet &out) const {
  return m_impl->visibilitySystem->TryBuildLayerVisibleCandidates(hiddenLayers, out);
}

bool Viewer3DController::TryBuildVisibleSet(
    const ViewFrustumSnapshot &frustum, bool useFrustumCulling,
    float minPixels, const VisibleSet &layerVisibleCandidates,
    VisibleSet &out) const {
  return m_impl->visibilitySystem->TryBuildVisibleSet(
      frustum, useFrustumCulling, minPixels, layerVisibleCandidates, out);
}

const Viewer3DController::VisibleSet &Viewer3DController::GetVisibleSet(
    const ViewFrustumSnapshot &frustum,
    const std::unordered_set<std::string> &hiddenLayers,
    bool useFrustumCulling, float minPixels) const {
  return m_impl->visibilitySystem->GetVisibleSet(frustum, hiddenLayers,
                                           useFrustumCulling, minPixels);
}

void Viewer3DController::RebuildVisibleSetCache() {
  m_impl->visibilitySystem->RebuildVisibleSetCache();
}

// Renders all scene objects using their transformMatrix
void Viewer3DController::RenderScene(bool wireframe, Viewer2DRenderMode mode,
                                     Viewer2DView view, bool showGrid,
                                     int gridStyle, float gridR, float gridG,
                                     float gridB, bool gridOnTop,
                                     bool is2DViewer) {
  ConfigManager &cfg = ConfigManager::Get();

  RenderFrameContext context;
  context.wireframe = wireframe;
  context.mode = mode;
  context.view = view;
  context.showGrid = showGrid;
  context.gridStyle = gridStyle;
  context.gridR = gridR;
  context.gridG = gridG;
  context.gridB = gridB;
  context.gridOnTop = gridOnTop;
  context.is2DViewer = is2DViewer;

  m_impl->useAdaptiveLineProfile =
      cfg.GetFloat("viewer3d_adaptive_line_profile") >= 0.5f;

  const bool skipOutlinesWhenMoving =
      cfg.GetFloat("viewer3d_skip_outlines_when_moving") >= 0.5f;
  const bool skipCaptureWhenMoving =
      cfg.GetFloat("viewer3d_skip_capture_when_moving") >= 0.5f;

  context.fastInteractionMode = IsFastInteractionModeEnabled(cfg);

  // During camera movement we prioritize frame pacing: keep drawing the
  // scene and camera updates, but defer optional CPU/GPU work until the
  // interaction grace period ends in Viewer3DPanel::ShouldPauseHeavyTasks().
  context.skipOptionalWork = m_impl->cameraMoving && context.fastInteractionMode;
  context.skipCapture = context.skipOptionalWork && skipCaptureWhenMoving;
  context.skipOutlinesForCurrentFrame =
      context.skipOptionalWork && skipOutlinesWhenMoving;

  const bool isTopView = context.view == Viewer2DView::Top;
  const bool isFrontView = context.view == Viewer2DView::Front;
  const bool isSideView = context.view == Viewer2DView::Side;
  const bool isBottomView = context.view == Viewer2DView::Bottom;

  // View mode flags are grouped here so the rest of RenderScene remains a
  // straight orchestration flow.
  (void)isTopView;
  (void)isFrontView;
  (void)isSideView;
  (void)isBottomView;

  const CullingSettings culling = GetCullingSettings3D(cfg);

  // 2D orthographic projections use a very different depth setup than the 3D
  // camera, and the frustum/pixel culling pass can incorrectly reject every
  // item even when all layers are visible. Keep layer-based filtering active
  // in 2D, but skip frustum culling there so visibility in the Layers panel
  // matches what is rendered on screen.
  context.useFrustumCulling = culling.enabled && !context.is2DViewer;
  context.minCullingPixels =
      context.is2DViewer ? culling.minPixels2D : culling.minPixels3D;

  const bool isWireframeMode =
      context.mode == Viewer2DRenderMode::Wireframe;
  const bool isWhiteMode = context.mode == Viewer2DRenderMode::White;
  const bool isByFixtureTypeMode =
      context.mode == Viewer2DRenderMode::ByFixtureType;
  const bool isByLayerMode = context.mode == Viewer2DRenderMode::ByLayer;

  context.useLighting = !context.wireframe;

  const bool shouldDrawGrid = context.showGrid;
  const bool shouldDrawGridBeforeScene = shouldDrawGrid && !context.gridOnTop;
  const bool shouldDrawGridAfterScene = shouldDrawGrid && context.gridOnTop;

  context.drawGridBeforeScene = shouldDrawGridBeforeScene;
  context.drawGridAfterScene = shouldDrawGridAfterScene;

  context.colorByFixtureType =
      context.wireframe && isByFixtureTypeMode;
  context.colorByLayer = context.wireframe && isByLayerMode;

  // Keep explicit mode flags local to the context build step to avoid
  // branching logic in the render execution code path.
  (void)isWireframeMode;
  (void)isWhiteMode;

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  context.hiddenLayers = hiddenLayers;

  RenderPipeline pipeline(*this);
  pipeline.Execute(context);
}

const Viewer3DController::VisibleSet &Viewer3DController::PrepareRenderFrame(
    const RenderFrameContext &context, ViewFrustumSnapshot &frustum) {
  m_impl->skipOutlinesForCurrentFrame = context.skipOutlinesForCurrentFrame;

  int viewport[4] = {0, 0, 0, 0};
  double model[16] = {0.0};
  double proj[16] = {0.0};
  if (context.useFrustumCulling) {
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
  }

  {
    const auto &sceneObjects = SceneDataManager::Instance().GetSceneObjects();
    const auto &trusses = SceneDataManager::Instance().GetTrusses();
    const auto &fixtures = SceneDataManager::Instance().GetFixtures();

    std::lock_guard<std::mutex> lock(m_impl->sortedListsMutex);
    if (m_impl->sortedListsDirty && !context.skipOptionalWork) {
      m_impl->sortedObjects.clear();
      m_impl->sortedObjects.reserve(sceneObjects.size());
      for (const auto &obj : sceneObjects)
        m_impl->sortedObjects.push_back(&obj);
      std::sort(m_impl->sortedObjects.begin(), m_impl->sortedObjects.end(),
                [](const auto *a, const auto *b) {
                  return a->second.transform.o[2] < b->second.transform.o[2];
                });

      m_impl->sortedTrusses.clear();
      m_impl->sortedTrusses.reserve(trusses.size());
      for (const auto &t : trusses)
        m_impl->sortedTrusses.push_back(&t);
      std::sort(m_impl->sortedTrusses.begin(), m_impl->sortedTrusses.end(),
                [](const auto *a, const auto *b) {
                  return a->second.transform.o[2] < b->second.transform.o[2];
                });

      m_impl->sortedFixtures.clear();
      m_impl->sortedFixtures.reserve(fixtures.size());
      for (const auto &f : fixtures)
        m_impl->sortedFixtures.push_back(&f);
      std::sort(m_impl->sortedFixtures.begin(), m_impl->sortedFixtures.end(),
                [](const auto *a, const auto *b) {
                  return a->second.transform.o[2] < b->second.transform.o[2];
                });

      m_impl->sortedListsDirty = false;
    }
  }

  std::copy(std::begin(viewport), std::end(viewport), std::begin(frustum.viewport));
  std::copy(std::begin(model), std::end(model), std::begin(frustum.model));
  std::copy(std::begin(proj), std::end(proj), std::begin(frustum.projection));

  return GetVisibleSet(frustum, context.hiddenLayers, context.useFrustumCulling,
                       context.minCullingPixels);
}

void Viewer3DController::RenderOpaqueFrame(const RenderFrameContext &context,
                                           const VisibleSet &visibleSet) {
  const bool wireframe = context.wireframe;
  const Viewer2DRenderMode mode = context.mode;
  const Viewer2DView view = context.view;

  if (context.useLighting)
    SetupBasicLighting();
  else
    glDisable(GL_LIGHTING);

  if (context.drawGridBeforeScene)
    DrawGrid(context.gridStyle, context.gridR, context.gridG, context.gridB,
             view);

  auto getTypeColor = [&](const std::string &key, const std::string &hex) {
    std::array<float, 3> c;
    if (!hex.empty() && HexToRGB(hex, c[0], c[1], c[2])) {
      m_impl->typeColors[key] = c;
      return c;
    }
    c = MakeDeterministicColor("type:" + key);
    m_impl->typeColors[key] = c;
    return c;
  };
  auto getLayerColor = [&](const std::string &key) {
    std::array<float, 3> c;
    auto opt = ConfigManager::Get().GetLayerColor(key);
    if (opt && HexToRGB(*opt, c[0], c[1], c[2])) {
      m_impl->layerColors[key] = c;
      return c;
    }
    c = MakeDeterministicColor("layer:" + key);
    m_impl->layerColors[key] = c;
    return c;
  };
  auto resolveSymbolView = [](Viewer2DView viewKind) {
    switch (viewKind) {
    case Viewer2DView::Top:
      return SymbolViewKind::Top;
    case Viewer2DView::Front:
      return SymbolViewKind::Front;
    case Viewer2DView::Side:
      return SymbolViewKind::Left;
    case Viewer2DView::Bottom:
    default:
      return SymbolViewKind::Bottom;
    }
  };

  OpaqueObjectPass::Render(*this, context, visibleSet, getLayerColor,
                           resolveSymbolView);
  OpaqueTrussPass::Render(*this, context, visibleSet, getLayerColor,
                          resolveSymbolView);
  OpaqueFixturePass::Render(*this, context, visibleSet, getTypeColor,
                            getLayerColor, resolveSymbolView);
}

void Viewer3DController::RenderOverlayFrame(const RenderFrameContext &context,
                                            const VisibleSet &visibleSet) {
  (void)visibleSet;
  if (context.drawGridAfterScene) {
    glDisable(GL_DEPTH_TEST);
    DrawGrid(context.gridStyle, context.gridR, context.gridG, context.gridB,
             context.view);
    glEnable(GL_DEPTH_TEST);
  }

  DrawAxes();
}

void Viewer3DController::FinalizeRenderFrame() {
  m_impl->skipOutlinesForCurrentFrame = false;
  if (m_impl->captureCanvas)
    m_impl->captureCanvas->SetSourceKey("unknown");
}


void Viewer3DController::SetDarkMode(bool enabled) { m_impl->darkMode = enabled; }

void Viewer3DController::SetInteracting(bool interacting) {
  const bool wasInteracting = m_impl->isInteracting;
  m_impl->isInteracting = interacting;
  if (wasInteracting && !m_impl->isInteracting)
    UpdateResourcesIfDirty();
}

void Viewer3DController::SetCameraMoving(bool moving) { m_impl->cameraMoving = moving; }

void Viewer3DController::SetSelectionOutlineEnabled(bool enabled) {
  m_impl->showSelectionOutline2D = enabled;
}

void Viewer3DController::SetCaptureCanvas(ICanvas2D *canvas, Viewer2DView view,
                                          bool includeGrid,
                                          bool useSymbolInstancing) {
  m_impl->captureCanvas = canvas;
  m_impl->captureView = view;
  m_impl->captureIncludeGrid = includeGrid;
  m_impl->captureUseSymbols = canvas ? useSymbolInstancing : false;
}

bool Viewer3DController::IsCameraMoving() const { return m_impl->cameraMoving; }

std::array<float, 3> Viewer3DController::AdjustColor(float r, float g,
                                                     float b) const {
  if (!m_impl->darkMode)
    return {r, g, b};
  return {r, g, b};
}

void Viewer3DController::SetGLColor(float r, float g, float b) const {
  auto adjusted = AdjustColor(r, g, b);
  glColor3f(adjusted[0], adjusted[1], adjusted[2]);
}

// Draws a solid cube centered at origin with given size and color
void Viewer3DController::DrawCube(float size, float r, float g, float b) {
  GLPrimitiveRenderer::DrawCube(
      size, r, g, b, m_impl->captureOnly,
      [this](float cr, float cg, float cb) { SetGLColor(cr, cg, cb); });
}


// Draws a wireframe cube centered at origin with given size and color
void Viewer3DController::DrawWireframeCube(
    float size, float r, float g, float b, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform,
    float lineWidthOverride, bool recordCapture) {
  float lineWidth =
      GetLineRenderProfile(m_impl->isInteracting, mode == Viewer2DRenderMode::Wireframe,
                           m_impl->useAdaptiveLineProfile)
          .lineWidth;
  GLPrimitiveRenderer::DrawWireframeCube(
      size, r, g, b, mode, captureTransform, lineWidth, lineWidthOverride,
      recordCapture, m_impl->captureOnly, m_impl->captureCanvas,
      [this](float cr, float cg, float cb) { SetGLColor(cr, cg, cb); },
      [this](const std::array<float, 3> &a, const std::array<float, 3> &b,
             const CanvasStroke &stroke) { RecordLine(a, b, stroke); });
}


// Draws a wireframe box whose origin sits at the left end of the span.
// The box extends along +X for the given length and is centered in Y/Z.
void Viewer3DController::DrawWireframeBox(
    float length, float height, float width, bool highlight, bool selected,
    bool wireframe, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  float lineWidth =
      GetLineRenderProfile(m_impl->isInteracting, mode == Viewer2DRenderMode::Wireframe,
                           m_impl->useAdaptiveLineProfile)
          .lineWidth;
  GLPrimitiveRenderer::DrawWireframeBox(
      length, height, width, highlight, selected, wireframe, mode,
      captureTransform, m_impl->skipOutlinesForCurrentFrame, m_impl->showSelectionOutline2D,
      m_impl->captureOnly, m_impl->captureCanvas, lineWidth,
      [this](float cr, float cg, float cb) { SetGLColor(cr, cg, cb); },
      [this](const std::array<float, 3> &a, const std::array<float, 3> &b,
             const CanvasStroke &stroke) { RecordLine(a, b, stroke); });
}


// Draws a colored cube. If selected or highlighted it is tinted
// in cyan or green respectively instead of its original color.
void Viewer3DController::DrawCubeWithOutline(
    float size, float r, float g, float b, bool highlight, bool selected,
    float cx, float cy, float cz, bool wireframe, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  (void)cx;
  (void)cy;
  (void)cz;

  float lineWidth =
      GetLineRenderProfile(m_impl->isInteracting, mode == Viewer2DRenderMode::Wireframe,
                           m_impl->useAdaptiveLineProfile)
          .lineWidth;
  GLPrimitiveRenderer::DrawCubeWithOutline(
      size, r, g, b, highlight, selected, wireframe, mode, captureTransform,
      m_impl->skipOutlinesForCurrentFrame, m_impl->showSelectionOutline2D, m_impl->captureOnly,
      m_impl->captureCanvas, lineWidth,
      [this](float cr, float cg, float cb) { SetGLColor(cr, cg, cb); },
      [this](const std::array<float, 3> &a, const std::array<float, 3> &b,
             const CanvasStroke &stroke) { RecordLine(a, b, stroke); },
      [this](const std::vector<std::array<float, 3>> &points,
             const CanvasStroke &stroke, const CanvasFill *fill) {
        RecordPolygon(points, stroke, fill);
      });
}


void Viewer3DController::SetupMeshBuffers(Mesh &mesh) {
  if (mesh.vertices.empty() || mesh.indices.empty())
    return;

  if (mesh.buffersReady)
    ReleaseMeshBuffers(mesh);

  EnsureOutwardWinding(mesh);

  if (mesh.normals.size() < mesh.vertices.size())
    ComputeNormals(mesh);

  std::vector<unsigned short> lineIndices =
      BuildWireframeIndices(mesh.vertices, mesh.indices);

  glGenVertexArrays(1, &mesh.vao);
  glBindVertexArray(mesh.vao);

  glGenBuffers(1, &mesh.vboVertices);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vboVertices);
  glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float),
               mesh.vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &mesh.vboNormals);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vboNormals);
  glBufferData(GL_ARRAY_BUFFER, mesh.normals.size() * sizeof(float),
               mesh.normals.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &mesh.eboTriangles);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               mesh.indices.size() * sizeof(unsigned short),
               mesh.indices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &mesh.eboLines);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboLines);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               lineIndices.size() * sizeof(unsigned short),
               lineIndices.data(), GL_STATIC_DRAW);

  // Restore the triangle index buffer while the VAO is bound so the VAO keeps
  // a valid default index buffer for solid rendering and future draws.
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.eboTriangles);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  const bool gpuResourcesValid =
      glIsVertexArray(mesh.vao) == GL_TRUE &&
      glIsBuffer(mesh.vboVertices) == GL_TRUE &&
      glIsBuffer(mesh.vboNormals) == GL_TRUE &&
      glIsBuffer(mesh.eboTriangles) == GL_TRUE &&
      glIsBuffer(mesh.eboLines) == GL_TRUE;

  if (!gpuResourcesValid) {
    mesh.vao = 0;
    mesh.vboVertices = 0;
    mesh.vboNormals = 0;
    mesh.eboTriangles = 0;
    mesh.eboLines = 0;
    mesh.triangleIndexCount = 0;
    mesh.lineIndexCount = 0;
    mesh.buffersReady = false;
    return;
  }

  mesh.triangleIndexCount = static_cast<int>(mesh.indices.size());
  mesh.lineIndexCount = static_cast<int>(lineIndices.size());
  mesh.buffersReady = true;
}

void Viewer3DController::ReleaseMeshBuffers(Mesh &mesh) {
  if (mesh.eboLines != 0) {
    glDeleteBuffers(1, &mesh.eboLines);
    mesh.eboLines = 0;
  }
  if (mesh.eboTriangles != 0) {
    glDeleteBuffers(1, &mesh.eboTriangles);
    mesh.eboTriangles = 0;
  }
  if (mesh.vboNormals != 0) {
    glDeleteBuffers(1, &mesh.vboNormals);
    mesh.vboNormals = 0;
  }
  if (mesh.vboVertices != 0) {
    glDeleteBuffers(1, &mesh.vboVertices);
    mesh.vboVertices = 0;
  }
  if (mesh.vao != 0) {
    glDeleteVertexArrays(1, &mesh.vao);
    mesh.vao = 0;
  }

  mesh.triangleIndexCount = 0;
  mesh.lineIndexCount = 0;
  mesh.buffersReady = false;
}

// Draws a mesh using the given color. When selected or highlighted the
// mesh is rendered entirely in cyan or green respectively.
// Draws a mesh using GL triangles. The optional scale parameter allows
// converting vertex units (e.g. millimeters) to meters.
// Draws a mesh using GL triangles. The optional scale parameter allows
// converting vertex units (e.g. millimeters) to meters.
// Draws the reference grid on one of the principal planes
// Draws the XYZ axes centered at origin
void Viewer3DController::DrawAxes() {
  const LineRenderProfile profile =
      GetLineRenderProfile(m_impl->isInteracting, false, m_impl->useAdaptiveLineProfile);
  const GLboolean lineSmoothWasEnabled = glIsEnabled(GL_LINE_SMOOTH);
  if (profile.enableLineSmoothing)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);

  glLineWidth(profile.lineWidth);
  glBegin(GL_LINES);
  SetGLColor(1.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(1.0f, 0.0f, 0.0f); // X
  SetGLColor(0.0f, 1.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 1.0f, 0.0f); // Y
  SetGLColor(0.0f, 0.0f, 1.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 1.0f); // Z
  glEnd();
  if (m_impl->captureCanvas) {
    CanvasStroke stroke;
    stroke.width = profile.lineWidth;
    stroke.color = {1.0f, 0.0f, 0.0f, 1.0f};
    RecordLine({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, stroke);
    stroke.color = {0.0f, 1.0f, 0.0f, 1.0f};
    RecordLine({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, stroke);
    stroke.color = {0.0f, 0.0f, 1.0f, 1.0f};
    RecordLine({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, stroke);
  }

  if (lineSmoothWasEnabled)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);
}

// Multiplies the current matrix by the given transform. When
// scaleTranslation is true the translation part is converted from
// millimeters to meters using RENDER_SCALE.
void Viewer3DController::ApplyTransform(const float matrix[16],
                                        bool scaleTranslation) {
  float m[16];
  std::copy(matrix, matrix + 16, m);
  if (scaleTranslation) {
    m[12] *= RENDER_SCALE;
    m[13] *= RENDER_SCALE;
    m[14] *= RENDER_SCALE;
  }
  glMultMatrixf(m);
}

// Initializes simple lighting for the scene
void Viewer3DController::SetupBasicLighting() {
  glEnable(GL_LIGHTING);
  // Keep normal lengths stable after model transforms with scaling.
  glEnable(GL_NORMALIZE);
  glEnable(GL_LIGHT0);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
  GLfloat diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
  GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
  GLfloat position[] = {2.0f, -4.0f, 5.0f, 0.0f};

  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
  glLightfv(GL_LIGHT0, GL_POSITION, position);

  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glShadeModel(GL_SMOOTH);
}

void Viewer3DController::DrawFixtureLabels(int width, int height) {
  m_impl->labelRenderSystem->DrawFixtureLabels(width, height);
}

// Renders labels for all fixtures in the current scene. Each label displays the
// fixture's instance name (or UUID), numeric ID and DMX address. The label
// position is determined by a configurable distance and angle from the fixture
// center so that, by default, labels appear slightly below the fixture in the
// 2D top-down view. The label width derives from the fixture bounds so the
// drawn tag roughly matches the fixture size, and the optional zoom parameter
// scales the label like regular geometry when zooming the 2D view.
void Viewer3DController::DrawAllFixtureLabels(int width, int height,
                                              Viewer2DView view, float zoom) {
  m_impl->labelRenderSystem->DrawAllFixtureLabels(width, height, view, zoom);
}

void Viewer3DController::DrawTrussLabels(int width, int height) {
  m_impl->labelRenderSystem->DrawTrussLabels(width, height);
}

void Viewer3DController::DrawSceneObjectLabels(int width, int height) {
  m_impl->labelRenderSystem->DrawSceneObjectLabels(width, height);
}

void Viewer3DController::SetLayerColor(const std::string &layer,
                                       const std::string &hex) {
  std::array<float, 3> c;
  if (HexToRGB(hex, c[0], c[1], c[2]))
    m_impl->layerColors[layer] = c;
  else
    m_impl->layerColors.erase(layer);
}

std::shared_ptr<const SymbolDefinitionSnapshot>
Viewer3DController::GetBottomSymbolCacheSnapshot() const {
  return m_impl->bottomSymbolCache.Snapshot();
}

void Viewer3DController::DrawMeshWithOutline(
    const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
    bool selected, float cx, float cy, float cz, bool wireframe,
    Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform,
    bool unlit, const float *modelMatrix) {
  m_impl->sceneRenderer->DrawMeshWithOutline(mesh, r, g, b, scale, highlight,
                                       selected, cx, cy, cz, wireframe, mode,
                                       captureTransform, unlit, modelMatrix);
}

void Viewer3DController::DrawMeshWireframe(
    const Mesh &mesh, float scale,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  m_impl->sceneRenderer->DrawMeshWireframe(mesh, scale, captureTransform);
}

void Viewer3DController::DrawMesh(const Mesh &mesh, float scale,
                                  const float *modelMatrix) {
  m_impl->sceneRenderer->DrawMesh(mesh, scale, modelMatrix);
}

void Viewer3DController::DrawGrid(int style, float r, float g, float b,
                                  Viewer2DView view) {
  m_impl->sceneRenderer->DrawGrid(style, r, g, b, view);
}

void Viewer3DController::SetupMaterialFromRGB(float r, float g, float b) {
  m_impl->sceneRenderer->SetupMaterialFromRGB(r, g, b);
}

bool Viewer3DController::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                           int height, wxString &outLabel,
                                           wxPoint &outPos,
                                           std::string *outUuid) {
  return m_impl->selectionSystem->GetFixtureLabelAt(mouseX, mouseY, width, height,
                                              outLabel, outPos, outUuid);
}

bool Viewer3DController::GetTrussLabelAt(int mouseX, int mouseY, int width,
                                         int height, wxString &outLabel,
                                         wxPoint &outPos,
                                         std::string *outUuid) {
  return m_impl->selectionSystem->GetTrussLabelAt(mouseX, mouseY, width, height,
                                            outLabel, outPos, outUuid);
}

bool Viewer3DController::GetSceneObjectLabelAt(int mouseX, int mouseY,
                                                int width, int height,
                                                wxString &outLabel,
                                                wxPoint &outPos,
                                                std::string *outUuid) {
  return m_impl->selectionSystem->GetSceneObjectLabelAt(mouseX, mouseY, width,
                                                  height, outLabel, outPos,
                                                  outUuid);
}

std::vector<std::string> Viewer3DController::GetFixturesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  return m_impl->selectionSystem->GetFixturesInScreenRect(x1, y1, x2, y2, width,
                                                    height);
}

std::vector<std::string> Viewer3DController::GetTrussesInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  return m_impl->selectionSystem->GetTrussesInScreenRect(x1, y1, x2, y2, width,
                                                   height);
}

std::vector<std::string> Viewer3DController::GetSceneObjectsInScreenRect(
    int x1, int y1, int x2, int y2, int width, int height) const {
  return m_impl->selectionSystem->GetSceneObjectsInScreenRect(x1, y1, x2, y2, width,
                                                        height);
}

bool Viewer3DController::IsInteracting() const { return m_impl->isInteracting; }

bool Viewer3DController::UseAdaptiveLineProfile() const {
  return m_impl->useAdaptiveLineProfile;
}

bool Viewer3DController::SkipOutlinesForCurrentFrame() const {
  return m_impl->skipOutlinesForCurrentFrame;
}

bool Viewer3DController::IsSelectionOutlineEnabled2D() const {
  return m_impl->showSelectionOutline2D;
}

bool Viewer3DController::IsCaptureOnly() const { return m_impl->captureOnly; }

ICanvas2D *Viewer3DController::GetCaptureCanvas() const {
  return m_impl->captureCanvas;
}

bool Viewer3DController::CaptureIncludesGrid() const {
  return m_impl->captureIncludeGrid;
}

const std::string &Viewer3DController::GetHighlightUuid() const {
  return m_impl->highlightUuid;
}

const std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetFixtureBoundsMap() const {
  return m_impl->fixtureBounds;
}

const std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetTrussBoundsMap() const {
  return m_impl->trussBounds;
}

const std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetObjectBoundsMap() const {
  return m_impl->objectBounds;
}

NVGcontext *Viewer3DController::GetNanoVGContext() const { return m_impl->vg; }

int Viewer3DController::GetLabelFont() const { return m_impl->font; }

int Viewer3DController::GetLabelBoldFont() const { return m_impl->fontBold; }

bool Viewer3DController::IsDarkMode() const { return m_impl->darkMode; }

ResourceSyncState &Viewer3DController::GetResourceSyncState() {
  return m_impl->resourceSyncState;
}

std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetModelBounds() {
  return m_impl->modelBounds;
}

std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetFixtureBounds() {
  return m_impl->fixtureBounds;
}

std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetTrussBounds() {
  return m_impl->trussBounds;
}

std::unordered_map<std::string, Viewer3DController::BoundingBox> &
Viewer3DController::GetObjectBounds() {
  return m_impl->objectBounds;
}

size_t Viewer3DController::GetSceneVersion() const { return m_impl->sceneVersion; }

const std::vector<const std::pair<const std::string, Fixture> *> &
Viewer3DController::GetSortedFixtures() const {
  return m_impl->sortedFixtures;
}

const std::vector<const std::pair<const std::string, Truss> *> &
Viewer3DController::GetSortedTrusses() const {
  return m_impl->sortedTrusses;
}

const std::vector<const std::pair<const std::string, SceneObject> *> &
Viewer3DController::GetSortedObjects() const {
  return m_impl->sortedObjects;
}

std::mutex &Viewer3DController::GetSortedListsMutex() const {
  return m_impl->sortedListsMutex;
}

Viewer3DController::VisibleSet &Viewer3DController::GetCachedVisibleSet() const {
  return m_impl->cachedVisibleSet;
}

Viewer3DController::VisibleSet &
Viewer3DController::GetCachedLayerVisibleCandidates() const {
  return m_impl->cachedLayerVisibleCandidates;
}

size_t &Viewer3DController::GetLayerVisibleCandidatesSceneVersion() const {
  return m_impl->layerVisibleCandidatesSceneVersion;
}

std::unordered_set<std::string> &
Viewer3DController::GetLayerVisibleCandidatesHiddenLayers() const {
  return m_impl->layerVisibleCandidatesHiddenLayers;
}

size_t &Viewer3DController::GetLayerVisibleCandidatesRevision() const {
  return m_impl->layerVisibleCandidatesRevision;
}

size_t &Viewer3DController::GetVisibleSetLayerCandidatesRevision() const {
  return m_impl->visibleSetLayerCandidatesRevision;
}

bool &Viewer3DController::GetVisibleSetFrustumCulling() const {
  return m_impl->visibleSetFrustumCulling;
}

float &Viewer3DController::GetVisibleSetMinPixels() const {
  return m_impl->visibleSetMinPixels;
}

std::array<int, 4> &Viewer3DController::GetVisibleSetViewport() const {
  return m_impl->visibleSetViewport;
}

std::array<double, 16> &Viewer3DController::GetVisibleSetModel() const {
  return m_impl->visibleSetModel;
}

std::array<double, 16> &Viewer3DController::GetVisibleSetProjection() const {
  return m_impl->visibleSetProjection;
}
