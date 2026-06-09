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

  return 0;
}
