#include "bounds_cache_system.h"

#include "../../core/projectutils.h"
#include "../../models/types.h"
#include "../../core/logger.h"
#include "../matrixutils.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

static std::string NormalizePath(const std::string &p) {
  std::string out = p;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  return out;
}

static std::string ResolveCacheKey(const std::string &pathRef) {
  return NormalizePath(pathRef);
}

static bool IsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                                 const std::string &layer) {
  if (layer.empty())
    return hidden.find(DEFAULT_LAYER_NAME) == hidden.end();
  return hidden.find(layer) == hidden.end();
}

static Viewer3DBoundingBox TransformBounds(const Viewer3DBoundingBox &local,
                                           const Matrix &m) {
  Viewer3DBoundingBox world;
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
}

} // namespace

void BoundsCacheSystem::RebuildIfDirty(
    Context &context,
    const std::unordered_set<std::string> &hiddenLayers,
    const std::unordered_map<std::string, Truss> &trusses,
    const std::unordered_map<std::string, SceneObject> &objects,
    const std::unordered_map<std::string, Fixture> &fixtures) {
  if (hiddenLayers != context.boundsHiddenLayers) {
    Logger::Instance().Log("visibility dirty reason: hidden layers changed vs bounds cache");
    context.visibilityChangedDirty = true;
  }

  if (!(context.sceneChangedDirty || context.assetsChangedDirty ||
        context.visibilityChangedDirty) &&
      context.cachedVersion == context.sceneVersion) {
    return;
  }

  if (context.visibilityChangedDirty) {
    const auto eraseHidden = [&](auto &boundsMap, const auto &items) {
      for (auto it = boundsMap.begin(); it != boundsMap.end();) {
        auto itemIt = items.find(it->first);
        if (itemIt == items.end() ||
            !IsLayerVisibleCached(hiddenLayers, itemIt->second.layer)) {
          it = boundsMap.erase(it);
        } else {
          ++it;
        }
      }
    };
    eraseHidden(context.fixtureBounds, fixtures);
    eraseHidden(context.trussBounds, trusses);
    eraseHidden(context.objectBounds, objects);
    context.boundsHiddenLayers = hiddenLayers;
    std::lock_guard<std::mutex> lock(context.sortedListsMutex);
    context.sortedListsDirty = true;
  }
  context.cachedVersion = context.sceneVersion;

  context.fixtureBounds.clear();
  for (const auto &[uuid, f] : fixtures) {
    if (!IsLayerVisibleCached(hiddenLayers, f.layer))
      continue;
    Viewer3DBoundingBox bb;
    Matrix fix = f.transform;
    fix.o[0] *= RENDER_SCALE;
    fix.o[1] *= RENDER_SCALE;
    fix.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    std::string gdtfPath;
    auto gdtfPathIt =
        context.resourceSyncState.resolvedGdtfSpecs.find(ResolveCacheKey(f.gdtfSpec));
    if (gdtfPathIt != context.resourceSyncState.resolvedGdtfSpecs.end() &&
        gdtfPathIt->second.attempted) {
      gdtfPath = gdtfPathIt->second.resolvedPath;
    }
    auto itg = context.resourceSyncState.loadedGdtf.find(gdtfPath);
    if (itg != context.resourceSyncState.loadedGdtf.end()) {
      auto bit = context.modelBounds.find(gdtfPath);
      if (bit == context.modelBounds.end()) {
        Viewer3DBoundingBox local;
        local.min = {FLT_MAX, FLT_MAX, FLT_MAX};
        local.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        bool localFound = false;
        for (const auto &obj : itg->second) {
          for (size_t vi = 0; vi + 2 < obj.mesh.vertices.size(); vi += 3) {
            std::array<float, 3> p = {
                obj.mesh.vertices[vi] * RENDER_SCALE,
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
          bit = context.modelBounds.emplace(gdtfPath, local).first;
      }
      if (bit != context.modelBounds.end()) {
        bb = TransformBounds(bit->second, fix);
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

    context.fixtureBounds[uuid] = bb;
  }

  context.trussBounds.clear();
  for (const auto &[uuid, t] : trusses) {
    if (!IsLayerVisibleCached(hiddenLayers, t.layer))
      continue;
    Viewer3DBoundingBox bb;
    Matrix tm = t.transform;
    tm.o[0] *= RENDER_SCALE;
    tm.o[1] *= RENDER_SCALE;
    tm.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!t.symbolFile.empty()) {
      std::string path;
      auto pathIt =
          context.resourceSyncState.resolvedModelRefs.find(ResolveCacheKey(t.symbolFile));
      if (pathIt != context.resourceSyncState.resolvedModelRefs.end() &&
          pathIt->second.attempted) {
        path = pathIt->second.resolvedPath;
      }
      auto it = context.resourceSyncState.loadedMeshes.find(path);
      if (it != context.resourceSyncState.loadedMeshes.end()) {
        auto bit = context.modelBounds.find(path);
        if (bit == context.modelBounds.end()) {
          Viewer3DBoundingBox local;
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
            bit = context.modelBounds.emplace(path, local).first;
        }
        if (bit != context.modelBounds.end()) {
          bb = TransformBounds(bit->second, tm);
          found = true;
        }
      }
    }

    if (!found) {
      float len = (t.lengthMm > 0 ? t.lengthMm * RENDER_SCALE : 0.3f);
      float halfy = (t.widthMm > 0 ? t.widthMm * RENDER_SCALE * 0.5f : 0.15f);
      float z1 = (t.heightMm > 0 ? t.heightMm * RENDER_SCALE : 0.3f);
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{0.0f, -halfy, 0.0f},
          {len, -halfy, 0.0f},
          {0.0f, halfy, 0.0f},
          {len, halfy, 0.0f},
          {0.0f, -halfy, z1},
          {len, -halfy, z1},
          {0.0f, halfy, z1},
          {len, halfy, z1}};
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

    context.trussBounds[uuid] = bb;
  }

  context.objectBounds.clear();
  for (const auto &[uuid, obj] : objects) {
    if (!IsLayerVisibleCached(hiddenLayers, obj.layer))
      continue;
    Viewer3DBoundingBox bb;
    Matrix tm = obj.transform;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!obj.geometries.empty()) {
      for (const auto &geo : obj.geometries) {
        std::string path;
        auto modelIt =
            context.resourceSyncState.resolvedModelRefs.find(ResolveCacheKey(geo.modelFile));
        if (modelIt != context.resourceSyncState.resolvedModelRefs.end() &&
            modelIt->second.attempted) {
          path = modelIt->second.resolvedPath;
        }
        auto it = context.resourceSyncState.loadedMeshes.find(path);
        if (it == context.resourceSyncState.loadedMeshes.end())
          continue;

        auto bit = context.modelBounds.find(path);
        if (bit == context.modelBounds.end()) {
          Viewer3DBoundingBox local;
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
            bit = context.modelBounds.emplace(path, local).first;
        }
        if (bit == context.modelBounds.end())
          continue;

        Matrix geoTm = MatrixUtils::Multiply(tm, geo.localTransform);
        geoTm.o[0] *= RENDER_SCALE;
        geoTm.o[1] *= RENDER_SCALE;
        geoTm.o[2] *= RENDER_SCALE;
        Viewer3DBoundingBox geoWorld = TransformBounds(bit->second, geoTm);
        bb.min[0] = std::min(bb.min[0], geoWorld.min[0]);
        bb.min[1] = std::min(bb.min[1], geoWorld.min[1]);
        bb.min[2] = std::min(bb.min[2], geoWorld.min[2]);
        bb.max[0] = std::max(bb.max[0], geoWorld.max[0]);
        bb.max[1] = std::max(bb.max[1], geoWorld.max[1]);
        bb.max[2] = std::max(bb.max[2], geoWorld.max[2]);
        found = true;
      }
    } else if (!obj.modelFile.empty()) {
      std::string path;
      auto modelIt =
          context.resourceSyncState.resolvedModelRefs.find(ResolveCacheKey(obj.modelFile));
      if (modelIt != context.resourceSyncState.resolvedModelRefs.end() &&
          modelIt->second.attempted) {
        path = modelIt->second.resolvedPath;
      }
      auto it = context.resourceSyncState.loadedMeshes.find(path);
      if (it != context.resourceSyncState.loadedMeshes.end()) {
        auto bit = context.modelBounds.find(path);
        if (bit == context.modelBounds.end()) {
          Viewer3DBoundingBox local;
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
            bit = context.modelBounds.emplace(path, local).first;
        }
        if (bit != context.modelBounds.end()) {
          Matrix scaledTm = tm;
          scaledTm.o[0] *= RENDER_SCALE;
          scaledTm.o[1] *= RENDER_SCALE;
          scaledTm.o[2] *= RENDER_SCALE;
          bb = TransformBounds(bit->second, scaledTm);
          found = true;
        }
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

    context.objectBounds[uuid] = bb;
  }

  context.sceneChangedDirty = false;
  context.assetsChangedDirty = false;
  context.visibilityChangedDirty = false;
}
