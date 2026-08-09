#pragma once

#include <memory>

enum class SketchTone { Dark, Mid, Light };

// Returns whether the current render context requires Sketch post-processing.
inline bool ShouldApplySketchPostProcess(bool sketchStyle, bool wireframe,
                                         bool idOnlyPass) {
  return sketchStyle && !wireframe && !idOnlyPass;
}

// Maps neutral-base luminance to the existing three-tone Sketch palette.
inline SketchTone QuantizeSketchLuminance(float luminance) {
  if (luminance <= 0.10f)
    return SketchTone::Dark;
  if (luminance <= 0.30f)
    return SketchTone::Mid;
  return SketchTone::Light;
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
