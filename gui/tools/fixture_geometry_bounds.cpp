#include "tools/fixture_geometry_bounds.h"

#include <vector>

#include "culling/gdtf_object_bounds.h"
#include "gdtfloader.h"
#include "viewer3d_types.h"

namespace tools {

// Computes renderer-consistent fixture mesh bounds for an exact GDTF mode.
bool ComputeFixtureGeometryBoundsMm(const std::string &gdtfPath,
                                    const std::string &gdtfMode,
                                    FixtureGeometryBounds &bounds,
                                    std::string &errorMessage) {
  bounds = FixtureGeometryBounds{};
  std::vector<GdtfObject> objects;
  if (!LoadGdtf(gdtfPath, objects, gdtfMode, &errorMessage))
    return false;

  Viewer3DBoundingBox boundsMeters;
  if (!viewer3d::culling::ComputeGdtfObjectBoundsMeters(objects,
                                                        boundsMeters)) {
    errorMessage =
        "Could not determine fixture geometry bounds from GDTF meshes.";
    return false;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    bounds.min[axis] = boundsMeters.min[axis] / RENDER_SCALE;
    bounds.max[axis] = boundsMeters.max[axis] / RENDER_SCALE;
  }
  bounds.valid = true;
  return true;
}

} // namespace tools
