#include "visibilitysystem.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
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

#include "../../core/configmanager.h"
#include "../../models/matrixutils.h"
#include "../../core/scenedatamanager.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace {

bool IsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                          const std::string &layer) {
  if (layer.empty())
    return hidden.find(DEFAULT_LAYER_NAME) == hidden.end();
  return hidden.find(layer) == hidden.end();
}

std::unordered_set<std::string> SnapshotHiddenLayers(const ConfigManager &cfg) {
  return cfg.GetHiddenLayers();
}


static std::string NormalizePath(const std::string &pathRef) {
  std::string out = pathRef;
  std::replace(out.begin(), out.end(), '\\', '/');
  return out;
}

static std::string ResolveCacheKey(const std::string &pathRef) {
  return NormalizePath(pathRef);
}

static std::array<float, 3> TransformPoint(const Matrix &m,
                                           const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

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

static bool ProjectBoundingBoxToScreen(const std::array<float, 3> &bbMin,
                                       const std::array<float, 3> &bbMax,
                                       int viewportHeight,
                                       const double model[16],
                                       const double proj[16],
                                       const int viewport[4],
                                       ScreenRect &outRect,
                                       bool &outAnyDepthVisible) {
  outRect = ScreenRect{};
  outAnyDepthVisible = false;
  bool projected = false;

  std::array<std::array<float, 3>, 8> corners = {
      std::array<float, 3>{bbMin[0], bbMin[1], bbMin[2]},
      {bbMax[0], bbMin[1], bbMin[2]},
      {bbMin[0], bbMax[1], bbMin[2]},
      {bbMax[0], bbMax[1], bbMin[2]},
      {bbMin[0], bbMin[1], bbMax[2]},
      {bbMax[0], bbMin[1], bbMax[2]},
      {bbMin[0], bbMax[1], bbMax[2]},
      {bbMax[0], bbMax[1], bbMax[2]}};

  for (const auto &c : corners) {
    double sx, sy, sz;
    if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
        GL_TRUE) {
      projected = true;
      outRect.minX = std::min(outRect.minX, sx);
      outRect.maxX = std::max(outRect.maxX, sx);
      const double sy2 = static_cast<double>(viewportHeight) - sy;
      outRect.minY = std::min(outRect.minY, sy2);
      outRect.maxY = std::max(outRect.maxY, sy2);
      if (sz >= 0.0 && sz <= 1.0)
        outAnyDepthVisible = true;
    }
  }

  return projected;
}

static bool ShouldCullByScreenRect(const ScreenRect &rect, int width,
                                   int height, float minPixels) {
  if (rect.maxX < 0.0 || rect.minX > static_cast<double>(width) ||
      rect.maxY < 0.0 || rect.minY > static_cast<double>(height)) {
    return true;
  }

  const double screenWidth = rect.maxX - rect.minX;
  const double screenHeight = rect.maxY - rect.minY;
  return screenWidth < static_cast<double>(minPixels) &&
         screenHeight < static_cast<double>(minPixels);
}

} // namespace

bool VisibilitySystem::EnsureBoundsComputed(
    const std::string &uuid, IVisibilityContext::ItemType type,
    const std::unordered_set<std::string> &hiddenLayers) {
  auto transformBounds = [](const IVisibilityContext::BoundingBox &local,
                            const Matrix &m) {
    IVisibilityContext::BoundingBox world;
    world.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    world.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    const auto &mn = local.min;
    const auto &mx = local.max;
    const std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{mn[0], mn[1], mn[2]},
        {mx[0], mn[1], mn[2]},
        {mn[0], mx[1], mn[2]},
        {mx[0], mx[1], mn[2]},
        {mn[0], mn[1], mx[2]},
        {mx[0], mn[1], mx[2]},
        {mn[0], mx[1], mx[2]},
        {mx[0], mx[1], mx[2]}};
    for (const auto &c : corners) {
      auto p = TransformPoint(m, c);
      world.min[0] = std::min(world.min[0], p[0]);
      world.min[1] = std::min(world.min[1], p[1]);
      world.min[2] = std::min(world.min[2], p[2]);
      world.max[0] = std::max(world.max[0], p[0]);
      world.max[1] = std::max(world.max[1], p[1]);
      world.max[2] = std::max(world.max[2], p[2]);
    }
    return world;
  };

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  const auto &objects = SceneDataManager::Instance().GetSceneObjects();

  if (type == IVisibilityContext::ItemType::Fixture) {
    if (m_controller.GetFixtureBounds().find(uuid) !=
        m_controller.GetFixtureBounds().end())
      return true;
    auto fit = fixtures.find(uuid);
    if (fit == fixtures.end() ||
        !IsLayerVisibleCached(hiddenLayers, fit->second.layer))
      return false;

    IVisibilityContext::BoundingBox bb;
    Matrix fix = fit->second.transform;
    fix.o[0] *= RENDER_SCALE;
    fix.o[1] *= RENDER_SCALE;
    fix.o[2] *= RENDER_SCALE;
    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    std::string gdtfPath;
    auto gdtfIt = m_controller.GetResourceSyncState().resolvedGdtfSpecs.find(
        ResolveCacheKey(fit->second.gdtfSpec));
    if (gdtfIt != m_controller.GetResourceSyncState().resolvedGdtfSpecs.end() &&
        gdtfIt->second.attempted)
      gdtfPath = gdtfIt->second.resolvedPath;
    auto itg = m_controller.GetResourceSyncState().loadedGdtf.find(gdtfPath);
    if (itg != m_controller.GetResourceSyncState().loadedGdtf.end()) {
      auto bit = m_controller.GetModelBounds().find(gdtfPath);
      if (bit == m_controller.GetModelBounds().end()) {
        IVisibilityContext::BoundingBox local;
        local.min = {FLT_MAX, FLT_MAX, FLT_MAX};
        local.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        bool localFound = false;
        for (const auto &obj : itg->second) {
          for (size_t vi = 0; vi + 2 < obj.mesh.vertices.size(); vi += 3) {
            std::array<float, 3> p = {obj.mesh.vertices[vi] * RENDER_SCALE,
                                      obj.mesh.vertices[vi + 1] * RENDER_SCALE,
                                      obj.mesh.vertices[vi + 2] * RENDER_SCALE};
            p = TransformPoint(obj.transform, p);
            local.min[0] = std::min(local.min[0], p[0]);
            local.min[1] = std::min(local.min[1], p[1]);
            local.min[2] = std::min(local.min[2], p[2]);
            local.max[0] = std::max(local.max[0], p[0]);
            local.max[1] = std::max(local.max[1], p[1]);
            local.max[2] = std::max(local.max[2], p[2]);
            localFound = true;
          }
        }
        if (localFound)
          bit = m_controller.GetModelBounds().emplace(gdtfPath, local).first;
      }
      if (bit != m_controller.GetModelBounds().end()) {
        bb = transformBounds(bit->second, fix);
        found = true;
      }
    }

    if (!found) {
      float half = 0.1f;
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{-half, -half, -half},
          {half, -half, -half},
          {-half, half, -half},
          {half, half, -half},
          {-half, -half, half},
          {half, -half, half},
          {-half, half, half},
          {half, half, half}};
      for (const auto &c : corners) {
        auto p = TransformPoint(fix, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }
    m_controller.GetFixtureBounds()[uuid] = bb;
    return true;
  }

  if (type == IVisibilityContext::ItemType::Truss) {
    if (m_controller.GetTrussBounds().find(uuid) != m_controller.GetTrussBounds().end())
      return true;
    auto tit = trusses.find(uuid);
    if (tit == trusses.end() ||
        !IsLayerVisibleCached(hiddenLayers, tit->second.layer))
      return false;

    IVisibilityContext::BoundingBox bb;
    Matrix tm = tit->second.transform;
    tm.o[0] *= RENDER_SCALE;
    tm.o[1] *= RENDER_SCALE;
    tm.o[2] *= RENDER_SCALE;
    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!tit->second.symbolFile.empty()) {
      std::string path;
      auto modelIt = m_controller.GetResourceSyncState().resolvedModelRefs.find(
          ResolveCacheKey(tit->second.symbolFile));
      if (modelIt != m_controller.GetResourceSyncState().resolvedModelRefs.end() &&
          modelIt->second.attempted)
        path = modelIt->second.resolvedPath;
      auto bit = m_controller.GetModelBounds().find(path);
      if (bit == m_controller.GetModelBounds().end()) {
        auto it = m_controller.GetResourceSyncState().loadedMeshes.find(path);
        if (it != m_controller.GetResourceSyncState().loadedMeshes.end()) {
          IVisibilityContext::BoundingBox local;
          local.min = {FLT_MAX, FLT_MAX, FLT_MAX};
          local.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
          bool localFound = false;
          for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
            std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                      it->second.vertices[vi + 1] * RENDER_SCALE,
                                      it->second.vertices[vi + 2] * RENDER_SCALE};
            local.min[0] = std::min(local.min[0], p[0]);
            local.min[1] = std::min(local.min[1], p[1]);
            local.min[2] = std::min(local.min[2], p[2]);
            local.max[0] = std::max(local.max[0], p[0]);
            local.max[1] = std::max(local.max[1], p[1]);
            local.max[2] = std::max(local.max[2], p[2]);
            localFound = true;
          }
          if (localFound)
            bit = m_controller.GetModelBounds().emplace(path, local).first;
        }
      }
      if (bit != m_controller.GetModelBounds().end()) {
        bb = transformBounds(bit->second, tm);
        found = true;
      }
    }

    if (!found) {
      float len = (tit->second.lengthMm > 0 ? tit->second.lengthMm : 1000.0f) *
                  RENDER_SCALE;
      float wid = (tit->second.widthMm > 0 ? tit->second.widthMm : 200.0f) *
                  RENDER_SCALE;
      float hgt = (tit->second.heightMm > 0 ? tit->second.heightMm : 200.0f) *
                  RENDER_SCALE;
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{0.0f, -wid * 0.5f, 0.0f},
          {len, -wid * 0.5f, 0.0f},
          {0.0f, wid * 0.5f, 0.0f},
          {len, wid * 0.5f, 0.0f},
          {0.0f, -wid * 0.5f, hgt},
          {len, -wid * 0.5f, hgt},
          {0.0f, wid * 0.5f, hgt},
          {len, wid * 0.5f, hgt}};
      for (const auto &c : corners) {
        auto p = TransformPoint(tm, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }
    m_controller.GetTrussBounds()[uuid] = bb;
    return true;
  }

  if (m_controller.GetObjectBounds().find(uuid) != m_controller.GetObjectBounds().end())
    return true;
  auto oit = objects.find(uuid);
  if (oit == objects.end() || !IsLayerVisibleCached(hiddenLayers, oit->second.layer))
    return false;

  IVisibilityContext::BoundingBox bb;
  Matrix tm = oit->second.transform;
  bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
  bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  bool found = false;

  if (!oit->second.geometries.empty()) {
    for (const auto &geo : oit->second.geometries) {
      std::string path;
      auto modelIt = m_controller.GetResourceSyncState().resolvedModelRefs.find(
          ResolveCacheKey(geo.modelFile));
      if (modelIt != m_controller.GetResourceSyncState().resolvedModelRefs.end() &&
          modelIt->second.attempted)
        path = modelIt->second.resolvedPath;
      auto bit = m_controller.GetModelBounds().find(path);
      if (bit == m_controller.GetModelBounds().end()) {
        auto it = m_controller.GetResourceSyncState().loadedMeshes.find(path);
        if (it != m_controller.GetResourceSyncState().loadedMeshes.end()) {
          IVisibilityContext::BoundingBox local;
          local.min = {FLT_MAX, FLT_MAX, FLT_MAX};
          local.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
          bool localFound = false;
          for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
            std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                      it->second.vertices[vi + 1] * RENDER_SCALE,
                                      it->second.vertices[vi + 2] * RENDER_SCALE};
            local.min[0] = std::min(local.min[0], p[0]);
            local.min[1] = std::min(local.min[1], p[1]);
            local.min[2] = std::min(local.min[2], p[2]);
            local.max[0] = std::max(local.max[0], p[0]);
            local.max[1] = std::max(local.max[1], p[1]);
            local.max[2] = std::max(local.max[2], p[2]);
            localFound = true;
          }
          if (localFound)
            bit = m_controller.GetModelBounds().emplace(path, local).first;
        }
      }
      if (bit == m_controller.GetModelBounds().end())
        continue;
      Matrix geoTm = MatrixUtils::Multiply(tm, geo.localTransform);
      geoTm.o[0] *= RENDER_SCALE;
      geoTm.o[1] *= RENDER_SCALE;
      geoTm.o[2] *= RENDER_SCALE;
      IVisibilityContext::BoundingBox geoWorld = transformBounds(bit->second, geoTm);
      bb.min[0] = std::min(bb.min[0], geoWorld.min[0]);
      bb.min[1] = std::min(bb.min[1], geoWorld.min[1]);
      bb.min[2] = std::min(bb.min[2], geoWorld.min[2]);
      bb.max[0] = std::max(bb.max[0], geoWorld.max[0]);
      bb.max[1] = std::max(bb.max[1], geoWorld.max[1]);
      bb.max[2] = std::max(bb.max[2], geoWorld.max[2]);
      found = true;
    }
  } else if (!oit->second.modelFile.empty()) {
    std::string path;
    auto modelIt =
        m_controller.GetResourceSyncState().resolvedModelRefs.find(ResolveCacheKey(oit->second.modelFile));
    if (modelIt != m_controller.GetResourceSyncState().resolvedModelRefs.end() &&
        modelIt->second.attempted)
      path = modelIt->second.resolvedPath;
    auto bit = m_controller.GetModelBounds().find(path);
    if (bit == m_controller.GetModelBounds().end()) {
      auto it = m_controller.GetResourceSyncState().loadedMeshes.find(path);
      if (it != m_controller.GetResourceSyncState().loadedMeshes.end()) {
        IVisibilityContext::BoundingBox local;
        local.min = {FLT_MAX, FLT_MAX, FLT_MAX};
        local.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        bool localFound = false;
        for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
          std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                    it->second.vertices[vi + 1] * RENDER_SCALE,
                                    it->second.vertices[vi + 2] * RENDER_SCALE};
          local.min[0] = std::min(local.min[0], p[0]);
          local.min[1] = std::min(local.min[1], p[1]);
          local.min[2] = std::min(local.min[2], p[2]);
          local.max[0] = std::max(local.max[0], p[0]);
          local.max[1] = std::max(local.max[1], p[1]);
          local.max[2] = std::max(local.max[2], p[2]);
          localFound = true;
        }
        if (localFound)
          bit = m_controller.GetModelBounds().emplace(path, local).first;
      }
    }
    if (bit != m_controller.GetModelBounds().end()) {
      Matrix scaledTm = tm;
      scaledTm.o[0] *= RENDER_SCALE;
      scaledTm.o[1] *= RENDER_SCALE;
      scaledTm.o[2] *= RENDER_SCALE;
      bb = transformBounds(bit->second, scaledTm);
      found = true;
    }
  }

  if (!found) {
    float half = 0.15f;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{-half, -half, -half},
        {half, -half, -half},
        {-half, half, -half},
        {half, half, -half},
        {-half, -half, half},
        {half, -half, half},
        {-half, half, half},
        {half, half, half}};
    for (const auto &c : corners) {
      auto p = TransformPoint(tm, c);
      bb.min[0] = std::min(bb.min[0], p[0]);
      bb.min[1] = std::min(bb.min[1], p[1]);
      bb.min[2] = std::min(bb.min[2], p[2]);
      bb.max[0] = std::max(bb.max[0], p[0]);
      bb.max[1] = std::max(bb.max[1], p[1]);
      bb.max[2] = std::max(bb.max[2], p[2]);
    }
  }

  m_controller.GetObjectBounds()[uuid] = bb;
  return true;
}

bool VisibilitySystem::TryBuildLayerVisibleCandidates(
    const std::unordered_set<std::string> &hiddenLayers,
    IVisibilityContext::VisibleSet &out) const {
  const auto &sceneObjects = SceneDataManager::Instance().GetSceneObjects();
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  std::lock_guard<std::mutex> lock(m_controller.GetSortedListsMutex());
  out.objectUuids.clear();
  out.trussUuids.clear();
  out.fixtureUuids.clear();

  out.objectUuids.reserve(m_controller.GetSortedObjects().size());
  for (const auto *entry : m_controller.GetSortedObjects()) {
    if (!entry)
      continue;
    const auto &uuid = entry->first;
    const auto &obj = entry->second;
    if (!IsLayerVisibleCached(hiddenLayers, obj.layer))
      continue;
    if (sceneObjects.find(uuid) != sceneObjects.end())
      out.objectUuids.push_back(uuid);
  }

  out.trussUuids.reserve(m_controller.GetSortedTrusses().size());
  for (const auto *entry : m_controller.GetSortedTrusses()) {
    if (!entry)
      continue;
    const auto &uuid = entry->first;
    const auto &truss = entry->second;
    if (!IsLayerVisibleCached(hiddenLayers, truss.layer))
      continue;
    if (trusses.find(uuid) != trusses.end())
      out.trussUuids.push_back(uuid);
  }

  out.fixtureUuids.reserve(m_controller.GetSortedFixtures().size());
  for (const auto *entry : m_controller.GetSortedFixtures()) {
    if (!entry)
      continue;
    const auto &uuid = entry->first;
    const auto &fixture = entry->second;
    if (!IsLayerVisibleCached(hiddenLayers, fixture.layer))
      continue;
    if (fixtures.find(uuid) != fixtures.end())
      out.fixtureUuids.push_back(uuid);
  }

  return true;
}

bool VisibilitySystem::TryBuildVisibleSet(
    const IVisibilityContext::ViewFrustumSnapshot &frustum,
    bool useFrustumCulling, float minPixels,
    const IVisibilityContext::VisibleSet &layerVisibleCandidates,
    IVisibilityContext::VisibleSet &out) const {
  out.objectUuids.clear();
  out.trussUuids.clear();
  out.fixtureUuids.clear();

  out.objectUuids.reserve(layerVisibleCandidates.objectUuids.size());
  for (const auto &uuid : layerVisibleCandidates.objectUuids) {
    if (useFrustumCulling) {
      auto bit = m_controller.GetObjectBounds().find(uuid);
      if (bit == m_controller.GetObjectBounds().end())
        continue;
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bit->second.min, bit->second.max,
                                      frustum.viewport[3], frustum.model,
                                      frustum.projection, frustum.viewport, rect,
                                      anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, frustum.viewport[2], frustum.viewport[3],
                                 minPixels)) {
        continue;
      }
    }
    out.objectUuids.push_back(uuid);
  }

  out.trussUuids.reserve(layerVisibleCandidates.trussUuids.size());
  for (const auto &uuid : layerVisibleCandidates.trussUuids) {
    if (useFrustumCulling) {
      auto bit = m_controller.GetTrussBounds().find(uuid);
      if (bit == m_controller.GetTrussBounds().end())
        continue;
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bit->second.min, bit->second.max,
                                      frustum.viewport[3], frustum.model,
                                      frustum.projection, frustum.viewport, rect,
                                      anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, frustum.viewport[2], frustum.viewport[3],
                                 minPixels)) {
        continue;
      }
    }
    out.trussUuids.push_back(uuid);
  }

  out.fixtureUuids.reserve(layerVisibleCandidates.fixtureUuids.size());
  for (const auto &uuid : layerVisibleCandidates.fixtureUuids) {
    if (useFrustumCulling) {
      auto bit = m_controller.GetFixtureBounds().find(uuid);
      if (bit == m_controller.GetFixtureBounds().end())
        continue;
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bit->second.min, bit->second.max,
                                      frustum.viewport[3], frustum.model,
                                      frustum.projection, frustum.viewport, rect,
                                      anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, frustum.viewport[2], frustum.viewport[3],
                                 minPixels)) {
        continue;
      }
    }
    out.fixtureUuids.push_back(uuid);
  }

  return true;
}

const IVisibilityContext::VisibleSet &VisibilitySystem::GetVisibleSet(
    const IVisibilityContext::ViewFrustumSnapshot &frustum,
    const std::unordered_set<std::string> &hiddenLayers,
    bool useFrustumCulling, float minPixels) const {
  const bool layerCandidatesCacheValid =
      (m_controller.GetLayerVisibleCandidatesSceneVersion() ==
       m_controller.GetSceneVersion()) &&
      (m_controller.GetLayerVisibleCandidatesHiddenLayers() == hiddenLayers);

  if (!layerCandidatesCacheValid) {
    IVisibilityContext::VisibleSet builtCandidates;
    if (TryBuildLayerVisibleCandidates(hiddenLayers, builtCandidates)) {
      m_controller.GetCachedLayerVisibleCandidates() = std::move(builtCandidates);
      m_controller.GetLayerVisibleCandidatesSceneVersion() =
          m_controller.GetSceneVersion();
      m_controller.GetLayerVisibleCandidatesHiddenLayers() = hiddenLayers;
      ++m_controller.GetLayerVisibleCandidatesRevision();
    }
  }

  const bool sameViewport = std::equal(
      std::begin(frustum.viewport), std::end(frustum.viewport),
      m_controller.GetVisibleSetViewport().begin());
  const bool sameModel =
      std::equal(std::begin(frustum.model), std::end(frustum.model),
                 m_controller.GetVisibleSetModel().begin());
  const bool sameProjection =
      std::equal(std::begin(frustum.projection), std::end(frustum.projection),
                 m_controller.GetVisibleSetProjection().begin());

  const bool cacheValid =
      (m_controller.GetVisibleSetLayerCandidatesRevision() ==
       m_controller.GetLayerVisibleCandidatesRevision()) &&
      (m_controller.GetVisibleSetFrustumCulling() == useFrustumCulling) &&
      (m_controller.GetVisibleSetMinPixels() == minPixels) && sameViewport &&
      sameModel && sameProjection;

  if (!cacheValid) {
    if (m_controller.GetCachedLayerVisibleCandidates().Empty()) {
      m_controller.GetCachedVisibleSet() = {};
      m_controller.GetVisibleSetLayerCandidatesRevision() =
          m_controller.GetLayerVisibleCandidatesRevision();
      m_controller.GetVisibleSetFrustumCulling() = useFrustumCulling;
      m_controller.GetVisibleSetMinPixels() = minPixels;
      std::copy(std::begin(frustum.viewport), std::end(frustum.viewport),
                m_controller.GetVisibleSetViewport().begin());
      std::copy(std::begin(frustum.model), std::end(frustum.model),
                m_controller.GetVisibleSetModel().begin());
      std::copy(std::begin(frustum.projection), std::end(frustum.projection),
                m_controller.GetVisibleSetProjection().begin());
      return m_controller.GetCachedVisibleSet();
    }

    IVisibilityContext::VisibleSet built;
    if (TryBuildVisibleSet(frustum, useFrustumCulling, minPixels,
                           m_controller.GetCachedLayerVisibleCandidates(), built)) {
      m_controller.GetCachedVisibleSet() = std::move(built);
      m_controller.GetVisibleSetLayerCandidatesRevision() =
          m_controller.GetLayerVisibleCandidatesRevision();
      m_controller.GetVisibleSetFrustumCulling() = useFrustumCulling;
      m_controller.GetVisibleSetMinPixels() = minPixels;
      std::copy(std::begin(frustum.viewport), std::end(frustum.viewport),
                m_controller.GetVisibleSetViewport().begin());
      std::copy(std::begin(frustum.model), std::end(frustum.model),
                m_controller.GetVisibleSetModel().begin());
      std::copy(std::begin(frustum.projection), std::end(frustum.projection),
                m_controller.GetVisibleSetProjection().begin());
    }
  }

  return m_controller.GetCachedVisibleSet();
}

void VisibilitySystem::RebuildVisibleSetCache() {
  ConfigManager &cfg = ConfigManager::Get();
  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const CullingSettings culling = GetCullingSettings3D(cfg);
  int viewport[4] = {0, 0, 0, 0};
  double model[16] = {0.0};
  double proj[16] = {0.0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);

  IVisibilityContext::ViewFrustumSnapshot frustum{};
  std::copy(std::begin(viewport), std::end(viewport), std::begin(frustum.viewport));
  std::copy(std::begin(model), std::end(model), std::begin(frustum.model));
  std::copy(std::begin(proj), std::end(proj), std::begin(frustum.projection));

  const float minCullingPixels = culling.minPixels3D;
  (void)GetVisibleSet(frustum, hiddenLayers, culling.enabled, minCullingPixels);
}
