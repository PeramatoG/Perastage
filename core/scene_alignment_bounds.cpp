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
#include "scene_alignment_bounds.h"
#include "geometry_bounds_resolver.h"
#include "filesystem_path_utils.h"
#include "gdtfloader.h"
#include "matrixutils.h"
#include "resources/resource_path_resolution.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <set>

namespace alignment {
namespace {
Bounds Empty() {
  const auto inf = std::numeric_limits<double>::infinity();
  return {{inf,inf,inf}, {-inf,-inf,-inf}};
}
void Include(Bounds &bounds, const Bounds &other) {
  for (int axis = 0; axis < 3; ++axis) {
    bounds.min[axis] = std::min(bounds.min[axis], other.min[axis]);
    bounds.max[axis] = std::max(bounds.max[axis], other.max[axis]);
  }
}
std::optional<Bounds> Model(const MvrScene &scene, const std::string &reference,
                           std::string &diagnostic) {
  const auto path = viewer3d::resources::ResolveSceneResourcePath(scene.basePath, reference);
  auto bounds = GeometryBoundsResolver::Resolve(PathUtils::PathFromUtf8(path), &diagnostic);
  if (!bounds) return {};
  Bounds result;
  for (int i = 0; i < 3; ++i) { result.min[i] = bounds->minMm[i]; result.max[i] = bounds->maxMm[i]; }
  return result;
}
std::optional<Bounds> Fixture(const MvrScene &scene, const std::string &spec,
                             const std::string &mode, std::string &diagnostic) {
  const auto resolved = viewer3d::resources::ResolveSceneResourcePath(scene.basePath, spec);
  const auto path = PathUtils::PathFromUtf8(resolved);
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) { diagnostic = "GDTF resource is unavailable: " + spec; return {}; }
  const auto time = std::filesystem::last_write_time(path, ec);
  if (ec) { diagnostic = "GDTF resource version is unavailable: " + spec; return {}; }
  using PortableFileTicks = std::chrono::duration<std::int64_t,std::nano>;
  const auto ticks = std::chrono::duration_cast<PortableFileTicks>(time.time_since_epoch()).count();
  const auto key = PathUtils::BuildFilesystemIdentityKey(path) + "|" + mode + "|" +
      std::to_string(size) + "|" + std::to_string(ticks);
  // Bounded derived cache; canonical GDTF ownership remains in the existing loader.
  static std::map<std::string, Bounds> cache;
  if (const auto found = cache.find(key); found != cache.end()) return found->second;
  GdtfGeometryTree tree;
  if (!LoadGdtfGeometryTree(resolved, tree, mode, &diagnostic)) return {};
  Bounds result = Empty();
  for (const auto &node : tree.nodes) {
    if (node.modelName.empty()) continue;
    if (!node.hasMesh || !node.physicalBounds || node.mesh.vertices.empty() || node.mesh.vertices.size()%3) {
      diagnostic = "GDTF model lacks physical mesh/primitive geometry: " + node.modelName;
      return {};
    }
    // GDTF loader vertices are millimeters, node translations are meters.
    Matrix transform = node.worldTransform;
    for (auto &coordinate : transform.o) coordinate *= 1000.0f;
    for (size_t i = 0; i < node.mesh.vertices.size(); i += 3) {
      Bounds point;
      for (int axis = 0; axis < 3; ++axis) {
        point.min[axis] = point.max[axis] = node.mesh.vertices[i+axis];
        if (!std::isfinite(point.min[axis])) { diagnostic = "Non-finite GDTF vertex."; return {}; }
      }
      const auto measured = TransformBounds(point, transform);
      if (!Valid(measured)) { diagnostic = "Non-finite GDTF geometry transform."; return {}; }
      Include(result, measured);
    }
  }
  if (!Valid(result)) { diagnostic = "GDTF contains no physical geometry."; return {}; }
  if (cache.size() >= 128) cache.clear();
  cache.emplace(key, result);
  return result;
}
std::optional<Bounds> Geometries(const MvrScene &scene, const std::vector<GeometryInstance> &instances,
                               const std::string &model, const Matrix &world, std::string &diagnostic) {
  Bounds result = Empty();
  if (instances.empty()) {
    const auto local = Model(scene, model, diagnostic);
    if (!local) return {};
    return TransformBounds(*local, world);
  }
  for (const auto &instance : instances) {
    const auto local = Model(scene, instance.modelFile, diagnostic);
    if (!local) return {};
    const auto transformed = TransformBounds(*local, MatrixUtils::Multiply(world, instance.localTransform));
    if (!Valid(transformed)) { diagnostic = "Invalid geometry instance transform."; return {}; }
    Include(result, transformed);
  }
  return result;
}
std::optional<Bounds> Resolve(const MvrScene &scene, const Target &target,
                             std::set<std::pair<MvrNodeType,std::string>> &visiting,
                             std::string &diagnostic) {
  if (!visiting.insert({target.type,target.uuid}).second) {
    diagnostic = "Cyclic or multiply owned group geometry: " + target.uuid; return {};
  }
  switch (target.type) {
  case MvrNodeType::Fixture: {
    const auto &object = scene.fixtures.at(target.uuid);
    const auto local = Fixture(scene, object.gdtfSpec, object.gdtfMode, diagnostic);
    if (!local) return {};
    return TransformBounds(*local, object.transform);
  }
  case MvrNodeType::Truss: {
    const auto &object = scene.trusses.at(target.uuid);
    if (!object.symbolFile.empty()) {
      const auto local = Model(scene, object.symbolFile, diagnostic);
      if (!local) return {};
      return TransformBounds(*local, object.transform);
    }
    const auto local = Fixture(scene, object.gdtfSpec, object.gdtfMode, diagnostic);
    if (!local) return {};
    return TransformBounds(*local, object.transform);
  }
  case MvrNodeType::Support: {
    const auto &object = scene.supports.at(target.uuid);
    if (!object.geometries.empty() || !object.modelFile.empty())
      return Geometries(scene, object.geometries, object.modelFile, object.transform, diagnostic);
    const auto local = Fixture(scene, object.gdtfSpec, object.gdtfMode, diagnostic);
    if (!local) return {};
    return TransformBounds(*local, object.transform);
  }
  case MvrNodeType::SceneObject: {
    const auto &object = scene.sceneObjects.at(target.uuid);
    return Geometries(scene, object.geometries, object.modelFile, object.transform, diagnostic);
  }
  case MvrNodeType::GroupObject: {
    Bounds result = Empty();
    const auto &group = scene.groupObjects.at(target.uuid);
    for (const auto *axis : {&group.transform.u,&group.transform.v,&group.transform.w,&group.transform.o})
      for (auto value : *axis) if (!std::isfinite(value)) {
        diagnostic = "Non-finite group world transform: " + target.uuid; return {};
      }
    if (group.children.empty()) { diagnostic = "Group has no physical descendants."; return {}; }
    for (const auto &child : group.children) {
      // Reject inconsistent ownership before two effective groups could move
      // the same physical descendant. This validates the existing hierarchy.
      auto parent = [&]() -> std::string {
        switch (child.type) {
        case MvrNodeType::Fixture: return scene.fixtures.at(child.uuid).parentGroupUuid;
        case MvrNodeType::Truss: return scene.trusses.at(child.uuid).parentGroupUuid;
        case MvrNodeType::Support: return scene.supports.at(child.uuid).parentGroupUuid;
        case MvrNodeType::SceneObject: return scene.sceneObjects.at(child.uuid).parentGroupUuid;
        case MvrNodeType::GroupObject: return scene.groupObjects.at(child.uuid).parentGroupUuid;
        }
        return {};
      };
      if (parent() != target.uuid) {
        diagnostic = "Inconsistent group child ownership: " + child.uuid; return {};
      }
      const auto bounds = Resolve(scene, {child.type,child.uuid}, visiting, diagnostic);
      if (!bounds || !Valid(*bounds)) return {};
      Include(result,*bounds);
    }
    return result;
  }
  }
  return {};
}
}
Bounds TransformBounds(const Bounds &local, const Matrix &world) {
  Bounds result = Empty();
  // Reject invalid matrices before min/max can hide NaN coordinates.
  for (const auto *v : {&world.u,&world.v,&world.w,&world.o})
    for (auto value : *v) if (!std::isfinite(value)) return result;
  if (!Valid(local)) return result;
  for (int corner = 0; corner < 8; ++corner) {
    std::array<double,3> p;
    for (int axis = 0; axis < 3; ++axis)
      p[axis] = (corner & (1 << axis)) ? local.max[axis] : local.min[axis];
    for (int axis = 0; axis < 3; ++axis) {
      const auto value = world.o[axis] + world.u[axis]*p[0] + world.v[axis]*p[1] + world.w[axis]*p[2];
      result.min[axis] = std::min(result.min[axis],value);
      result.max[axis] = std::max(result.max[axis],value);
    }
  }
  return result;
}
std::optional<Bounds> SceneBounds(const MvrScene &scene, const Target &target, std::string &diagnostic) {
  try {
    std::set<std::pair<MvrNodeType,std::string>> visiting;
    auto result = Resolve(scene,target,visiting,diagnostic);
    if (result && !Valid(*result)) { diagnostic = "Invalid transformed physical bounds."; return {}; }
    return result;
  } catch (const std::exception &e) { diagnostic = e.what(); return {}; }
}
} // namespace alignment
