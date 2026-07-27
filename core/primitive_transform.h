#pragma once

#include "types.h"

#include <algorithm>

namespace PrimitiveTransform {

inline constexpr float kCanonicalCubeSizeMeters = 1.0f;
inline constexpr float kMinimumDimensionMeters = 0.01f;

// Builds scale relative to the canonical one-meter cube with X/Y/Z dimensions
// in meters.
inline Matrix BuildCubeScale(float sizeXMeters, float sizeYMeters,
                             float sizeZMeters) {
  Matrix transform;
  transform.u = {std::max(sizeXMeters, kMinimumDimensionMeters) /
                     kCanonicalCubeSizeMeters,
                 0.0f, 0.0f};
  transform.v = {0.0f,
                 std::max(sizeYMeters, kMinimumDimensionMeters) /
                     kCanonicalCubeSizeMeters,
                 0.0f};
  transform.w = {0.0f, 0.0f,
                 std::max(sizeZMeters, kMinimumDimensionMeters) /
                     kCanonicalCubeSizeMeters};
  return transform;
}

} // namespace PrimitiveTransform
