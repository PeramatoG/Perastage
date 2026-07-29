#include "console_command_parser.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

// Compares two float vectors with a small tolerance for parser regression tests.
bool EqualValues(const std::vector<float> &actual,
                 const std::vector<float> &expected) {
  if (actual.size() != expected.size())
    return false;
  for (size_t i = 0; i < actual.size(); ++i) {
    if (std::fabs(actual[i] - expected[i]) > 0.0001f)
      return false;
  }
  return true;
}

} // namespace

// Verifies transform value parsing accepts whitespace and thru-style separators.
int main() {
  bool relative = false;
  auto values = gui::console::ParseTransformValues("-7 7", relative);
  assert(!relative);
  assert(EqualValues(values, {-7.0f, 7.0f}));

  values = gui::console::ParseTransformValues("-7 t 7", relative);
  assert(!relative);
  assert(EqualValues(values, {-7.0f, 7.0f}));

  values = gui::console::ParseTransformValues("-7 thru 7", relative);
  assert(!relative);
  assert(EqualValues(values, {-7.0f, 7.0f}));

  values = gui::console::ParseTransformValues("++ 1.5 t 3.5", relative);
  assert(relative);
  assert(EqualValues(values, {1.5f, 3.5f}));

  values = gui::console::ParseTransformValues("-- 1.5 thru 3.5", relative);
  assert(relative);
  assert(EqualValues(values, {-1.5f, -3.5f}));

  auto localLong = gui::console::ParseTransformCommandSegment("++ 1 --local");
  assert(localLong.relative);
  assert(localLong.space == transform_space::TransformSpace::Local);
  auto localShort = gui::console::ParseTransformCommandSegment("++ 2 -l");
  assert(localShort.space == transform_space::TransformSpace::Local);
  auto grouped = gui::console::ParseTransformCommandSegment("++ 30 --group -l");
  assert(grouped.group);
  assert(grouped.space == transform_space::TransformSpace::Local);
  auto negative = gui::console::ParseTransformCommandSegment("-7 thru 7");
  assert(EqualValues(negative.values, {-7.0f, 7.0f}));
  auto relativeNegative = gui::console::ParseTransformCommandSegment("-- 1.5");
  assert(relativeNegative.relative);
  assert(EqualValues(relativeNegative.values, {-1.5f}));
  auto compactPositive = gui::console::ParseTransformCommandSegment("++1.25");
  assert(compactPositive.relative);
  assert(EqualValues(compactPositive.values, {1.25f}));
  auto compactNegative = gui::console::ParseTransformCommandSegment("--1.25");
  assert(compactNegative.relative);
  assert(EqualValues(compactNegative.values, {-1.25f}));
  auto compactRange =
      gui::console::ParseTransformCommandSegment("--1.5 thru 3.5 --local");
  assert(compactRange.relative);
  assert(compactRange.space == transform_space::TransformSpace::Local);
  assert(EqualValues(compactRange.values, {-1.5f, -3.5f}));
  auto modifierOnly = gui::console::ParseTransformCommandSegment("--group");
  assert(!modifierOnly.relative);
  assert(modifierOnly.group);
  auto malformed = gui::console::ParseTransformCommandSegment("--wat");
  assert(!malformed.relative);
  assert(malformed.values.empty());
  assert(malformed.remainder == "--wat");

  return 0;
}
