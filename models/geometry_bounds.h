#pragma once

#include <array>
#include <cmath>

// Stores a finite local-space geometry bounding box in millimeters.
struct GeometryBounds {
  std::array<float, 3> minMm{};
  std::array<float, 3> maxMm{};

  // Reports whether every axis has a finite, strictly positive extent.
  bool IsValid() const {
    for (int axis = 0; axis < 3; ++axis) {
      if (!std::isfinite(minMm[axis]) || !std::isfinite(maxMm[axis]) ||
          maxMm[axis] <= minMm[axis])
        return false;
    }
    return true;
  }

  // Returns the local-space extent of each axis in millimeters.
  std::array<float, 3> SizeMm() const {
    return {maxMm[0] - minMm[0], maxMm[1] - minMm[1],
            maxMm[2] - minMm[2]};
  }

  // Returns the local-space center of the bounding box in millimeters.
  std::array<float, 3> CenterMm() const {
    return {(minMm[0] + maxMm[0]) * 0.5f,
            (minMm[1] + maxMm[1]) * 0.5f,
            (minMm[2] + maxMm[2]) * 0.5f};
  }
};
