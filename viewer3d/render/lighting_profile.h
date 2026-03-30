#pragma once

namespace Viewer3DLightingProfile {

struct LightingOptions {
  bool ambientOcclusionEnabled = true;
  float ambientOcclusionStrength = 1.0f;
  bool whiteModelStyle = false;
};

void ApplyEnhancedBasicLighting(const LightingOptions &options);

} // namespace Viewer3DLightingProfile
