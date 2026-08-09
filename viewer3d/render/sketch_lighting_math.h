#pragma once

#include <array>

namespace Viewer3DSketchLighting {

// Returns the normal orientation used by fixed-function two-sided lighting.
inline std::array<float, 3>
OrientNormalForFace(const std::array<float, 3> &normal, bool frontFacing,
                    bool twoSidedLighting) {
  if (frontFacing || !twoSidedLighting)
    return normal;
  return {-normal[0], -normal[1], -normal[2]};
}

} // namespace Viewer3DSketchLighting
