#pragma once

#include "gdtf_geometry_types.h"
#include "viewer3d_types.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <vector>

namespace viewer3d::culling {

// Computes GDTF object bounds in the renderer's metre-based world domain.
inline bool ComputeGdtfObjectBoundsMeters(
    const std::vector<GdtfObject> &objects, Viewer3DBoundingBox &outBounds) {
  Viewer3DBoundingBox bounds;
  bounds.min = {FLT_MAX, FLT_MAX, FLT_MAX};
  bounds.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  bool found = false;

  for (const auto &object : objects) {
    for (size_t vi = 0; vi + 2 < object.mesh.vertices.size(); vi += 3) {
      const std::array<float, 3> local = {
          object.mesh.vertices[vi] * RENDER_SCALE,
          object.mesh.vertices[vi + 1] * RENDER_SCALE,
          object.mesh.vertices[vi + 2] * RENDER_SCALE};
      const std::array<float, 3> point = {
          object.transform.u[0] * local[0] +
              object.transform.v[0] * local[1] +
              object.transform.w[0] * local[2] + object.transform.o[0],
          object.transform.u[1] * local[0] +
              object.transform.v[1] * local[1] +
              object.transform.w[1] * local[2] + object.transform.o[1],
          object.transform.u[2] * local[0] +
              object.transform.v[2] * local[1] +
              object.transform.w[2] * local[2] + object.transform.o[2]};
      for (size_t axis = 0; axis < 3; ++axis) {
        bounds.min[axis] = std::min(bounds.min[axis], point[axis]);
        bounds.max[axis] = std::max(bounds.max[axis], point[axis]);
      }
      found = true;
    }
  }

  if (found)
    outBounds = bounds;
  return found;
}

} // namespace viewer3d::culling
