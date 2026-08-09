#pragma once

#include <memory>

enum class SketchTone { Dark, Mid, Light };

inline constexpr float kSketchDarkLuminanceThreshold = 0.40f;
inline constexpr float kSketchLightLuminanceThreshold = 0.72f;

// Returns whether the current render context requires Sketch post-processing.
inline bool ShouldApplySketchPostProcess(bool sketchStyle, bool wireframe,
                                         bool idOnlyPass) {
  return sketchStyle && !wireframe && !idOnlyPass;
}

// Maps neutral-base luminance to the existing three-tone Sketch palette.
inline SketchTone QuantizeSketchLuminance(float luminance) {
  if (luminance <= kSketchDarkLuminanceThreshold)
    return SketchTone::Dark;
  if (luminance <= kSketchLightLuminanceThreshold)
    return SketchTone::Mid;
  return SketchTone::Light;
}

// Converts the intermediate alpha marker into surviving black ink coverage.
inline float SketchInkCoverage(float alpha) {
  if (alpha <= 0.0f)
    return 1.0f;
  if (alpha >= 1.0f)
    return 0.0f;
  return 1.0f - alpha;
}

class SketchPostProcessPass {
public:
  SketchPostProcessPass();
  ~SketchPostProcessPass();

  SketchPostProcessPass(const SketchPostProcessPass &) = delete;
  SketchPostProcessPass &operator=(const SketchPostProcessPass &) = delete;

  bool Begin();
  void Composite();

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
