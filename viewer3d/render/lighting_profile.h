#pragma once

#include <array>
#include <cmath>

namespace Viewer3DLightingProfile {

inline constexpr std::array<float, 3> kKeyLightWorldDirection = {2.0f, -4.0f,
                                                                 5.0f};
inline constexpr std::array<float, 3> kFillLightWorldDirection = {-1.5f, 2.0f,
                                                                  1.0f};

struct LightingOptions {
  bool ambientOcclusionEnabled = true;
  float ambientOcclusionStrength = 1.0f;
  bool whiteModelStyle = false;
};

struct LightingState {
  std::array<float, 3> keyLightEyeDirection = kKeyLightWorldDirection;
  std::array<float, 3> fillLightEyeDirection = kFillLightWorldDirection;
  float keyDiffuseWeight = 0.78f;
  float fillDiffuseWeight = 0.32f;
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

// Converts a linear RGB diffuse color to its relative luminance.
inline float DiffuseLuminance(float red, float green, float blue) {
  return 0.2126f * red + 0.7152f * green + 0.0722f * blue;
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

// Evaluates the normalized directional diffuse field shared by Standard and Sketch.
inline float CombinedDirectionalDiffuse(
    const std::array<float, 3> &normal, const LightingState &lightingState) {
  const auto dot = [&normal](const std::array<float, 3> &direction) {
    const float value = normal[0] * direction[0] +
                        normal[1] * direction[1] +
                        normal[2] * direction[2];
    return value > 0.0f ? value : 0.0f;
  };
  const float weightSum =
      lightingState.keyDiffuseWeight + lightingState.fillDiffuseWeight;
  if (weightSum <= 1e-6f)
    return 0.0f;
  return (lightingState.keyDiffuseWeight *
              dot(lightingState.keyLightEyeDirection) +
          lightingState.fillDiffuseWeight *
              dot(lightingState.fillLightEyeDirection)) /
         weightSum;
}

LightingState ApplyEnhancedBasicLighting(const LightingOptions &options);

} // namespace Viewer3DLightingProfile
