#include "tools/fixture_geometry_bounds.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <vector>

#include "gdtfloader.h"
#include "types.h"

namespace tools {
namespace {

// Transforms a mesh point into fixture space.
std::array<float, 3> TransformPoint(const Matrix &m,
                                    const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}

// Expands bounds to include one transformed point.
void ExtendBounds(FixtureGeometryBounds &bounds,
                  const std::array<float, 3> &p) {
  if (!bounds.valid) {
    bounds.min = p;
    bounds.max = p;
    bounds.valid = true;
    return;
  }
  bounds.min[0] = std::min(bounds.min[0], p[0]);
  bounds.min[1] = std::min(bounds.min[1], p[1]);
  bounds.min[2] = std::min(bounds.min[2], p[2]);
  bounds.max[0] = std::max(bounds.max[0], p[0]);
  bounds.max[1] = std::max(bounds.max[1], p[1]);
  bounds.max[2] = std::max(bounds.max[2], p[2]);
}

} // namespace

// Computes renderer-consistent fixture mesh bounds for an exact GDTF mode.
bool ComputeFixtureGeometryBoundsMm(const std::string &gdtfPath,
                                    const std::string &gdtfMode,
                                    FixtureGeometryBounds &bounds,
                                    std::string &errorMessage) {
  bounds = FixtureGeometryBounds{};
  std::vector<GdtfObject> objects;
  if (!LoadGdtf(gdtfPath, objects, gdtfMode, &errorMessage))
    return false;

  for (const auto &object : objects) {
    for (size_t vi = 0; vi + 2 < object.mesh.vertices.size(); vi += 3) {
      const std::array<float, 3> local = {object.mesh.vertices[vi],
                                          object.mesh.vertices[vi + 1],
                                          object.mesh.vertices[vi + 2]};
      ExtendBounds(bounds, TransformPoint(object.transform, local));
    }
  }

  if (!bounds.valid) {
    errorMessage =
        "Could not determine fixture geometry bounds from GDTF meshes.";
    return false;
  }
  return true;
}

} // namespace tools
