#pragma once

namespace Viewer3DLightingProfile {

struct LightingOptions {
  bool ambientOcclusionEnabled = true;
};

void ApplyEnhancedBasicLighting(const LightingOptions &options);

} // namespace Viewer3DLightingProfile
