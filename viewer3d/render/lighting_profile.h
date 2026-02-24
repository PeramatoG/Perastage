#pragma once

namespace Viewer3DLightingProfile {

struct LightingOptions {
  bool ambientOcclusionEnabled = true;
  float ambientOcclusionStrength = 1.0f;
};

void ApplyEnhancedBasicLighting(const LightingOptions &options);

} // namespace Viewer3DLightingProfile
