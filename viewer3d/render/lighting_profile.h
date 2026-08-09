#pragma once

#include <array>
#include <cmath>

namespace Viewer3DLightingProfile {

inline constexpr std::array<float, 3> kKeyLightWorldDirection = {2.0f, -4.0f,
                                                                 5.0f};

struct LightingOptions {
  bool ambientOcclusionEnabled = true;
  float ambientOcclusionStrength = 1.0f;
  bool whiteModelStyle = false;
};

struct LightingState {
  std::array<float, 3> keyLightEyeDirection = kKeyLightWorldDirection;
  bool twoSidedLighting = true;
};

// Returns a normalized direction with a stable fallback for zero length.
inline std::array<float, 3>
NormalizeDirection(const std::array<float, 3> &direction) {
  const float length = std::sqrt(direction[0] * direction[0] +
                                 direction[1] * direction[1] +
                                 direction[2] * direction[2]);
  if (length <= 1e-6f)
    return {0.0f, 0.0f, 1.0f};
  return {direction[0] / length, direction[1] / length,
          direction[2] / length};
}

// Transforms a world-space direction into eye space using only view rotation.
inline std::array<float, 3>
TransformWorldDirectionToEyeSpace(const std::array<float, 3> &worldDirection,
                                  const float viewMatrix[16]) {
  return NormalizeDirection(
      {viewMatrix[0] * worldDirection[0] +
           viewMatrix[4] * worldDirection[1] +
           viewMatrix[8] * worldDirection[2],
       viewMatrix[1] * worldDirection[0] +
           viewMatrix[5] * worldDirection[1] +
           viewMatrix[9] * worldDirection[2],
       viewMatrix[2] * worldDirection[0] +
           viewMatrix[6] * worldDirection[1] +
           viewMatrix[10] * worldDirection[2]});
}

LightingState ApplyEnhancedBasicLighting(const LightingOptions &options);

} // namespace Viewer3DLightingProfile
