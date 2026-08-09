#include "render/sketch_post_process_pass.h"

#include <cassert>

// Verifies Sketch-only selection and three-tone quantization boundaries.
int main() {
  assert(ShouldApplySketchPostProcess(true, false, false));
  assert(!ShouldApplySketchPostProcess(false, false, false));
  assert(!ShouldApplySketchPostProcess(true, true, false));
  assert(!ShouldApplySketchPostProcess(true, false, true));

  assert(QuantizeSketchLuminance(0.0f) == SketchTone::Dark);
  assert(QuantizeSketchLuminance(kSketchDarkLuminanceThreshold) ==
         SketchTone::Dark);
  assert(QuantizeSketchLuminance(kSketchDarkLuminanceThreshold + 0.0001f) ==
         SketchTone::Mid);
  assert(QuantizeSketchLuminance(kSketchLightLuminanceThreshold) ==
         SketchTone::Mid);
  assert(QuantizeSketchLuminance(kSketchLightLuminanceThreshold + 0.0001f) ==
         SketchTone::Light);
  assert(QuantizeSketchLuminance(1.0f) == SketchTone::Light);
  assert(SketchInkCoverage(0.0f) == 1.0f);
  assert(SketchInkCoverage(0.25f) == 0.75f);
  assert(SketchInkCoverage(1.0f) == 0.0f);
  return 0;
}
