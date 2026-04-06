#include "primitive_bounds_utils.h"

#include "mesh.h"
#include "meshprimitives.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

constexpr std::string_view kPrimitivePrefix = "primitive:";

bool BuildBoundsFromMesh(const Mesh &mesh, Viewer3DBoundingBox &outBounds) {
  if (mesh.vertices.size() < 3)
    return false;

  Viewer3DBoundingBox bounds;
  bounds.min = {FLT_MAX, FLT_MAX, FLT_MAX};
  bounds.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

  bool found = false;
  for (size_t vi = 0; vi + 2 < mesh.vertices.size(); vi += 3) {
    const std::array<float, 3> p = {
        mesh.vertices[vi] * RENDER_SCALE,
        mesh.vertices[vi + 1] * RENDER_SCALE,
        mesh.vertices[vi + 2] * RENDER_SCALE,
    };
    bounds.min[0] = std::min(bounds.min[0], p[0]);
    bounds.min[1] = std::min(bounds.min[1], p[1]);
    bounds.min[2] = std::min(bounds.min[2], p[2]);
    bounds.max[0] = std::max(bounds.max[0], p[0]);
    bounds.max[1] = std::max(bounds.max[1], p[1]);
    bounds.max[2] = std::max(bounds.max[2], p[2]);
    found = true;
  }

  if (!found)
    return false;

  outBounds = bounds;
  return true;
}

} // namespace

bool TryGetPrimitiveBoundsFromModelRef(const std::string &modelRef,
                                       Viewer3DBoundingBox &outBounds) {
  if (modelRef.rfind(kPrimitivePrefix.data(), 0) != 0)
    return false;

  const std::string primitiveType = modelRef.substr(kPrimitivePrefix.size());
  if (primitiveType.empty())
    return false;

  static std::unordered_map<std::string, Viewer3DBoundingBox> boundsCache;
  const auto found = boundsCache.find(primitiveType);
  if (found != boundsCache.end()) {
    outBounds = found->second;
    return true;
  }

  Mesh primitiveMesh;
  if (!BuildPrimitiveMesh(primitiveType, primitiveMesh))
    return false;

  Viewer3DBoundingBox primitiveBounds;
  if (!BuildBoundsFromMesh(primitiveMesh, primitiveBounds))
    return false;

  boundsCache.emplace(primitiveType, primitiveBounds);
  outBounds = primitiveBounds;
  return true;
}
