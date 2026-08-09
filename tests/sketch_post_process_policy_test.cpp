#include "render/sketch_post_process_pass.h"

#include <cassert>

// Verifies Sketch-only selection and three-tone quantization boundaries.
int main() {
  assert(ShouldApplySketchPostProcess(true, false, false));
  assert(!ShouldApplySketchPostProcess(false, false, false));
  assert(!ShouldApplySketchPostProcess(true, true, false));
  assert(!ShouldApplySketchPostProcess(true, false, true));

  assert(QuantizeSketchLuminance(0.0f) == SketchTone::Dark);
  assert(QuantizeSketchLuminance(0.10f) == SketchTone::Dark);
  assert(QuantizeSketchLuminance(0.1001f) == SketchTone::Mid);
  assert(QuantizeSketchLuminance(0.30f) == SketchTone::Mid);
  assert(QuantizeSketchLuminance(0.3001f) == SketchTone::Light);
  assert(QuantizeSketchLuminance(1.0f) == SketchTone::Light);
  return 0;
}
