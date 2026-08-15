#include "interaction/hang_line_screen_projection.h"

#include <cmath>
#include <iostream>

namespace {

// Reports a failed test expectation.
bool Expect(bool condition, const char *message) {
  if (condition)
    return true;
  std::cerr << "FAILED: " << message << '\n';
  return false;
}

} // namespace

// Verifies screen-space endpoint selection maps onto the finite 3D hang line.
int main() {
  bool ok = true;
  const auto middle = viewer3d::ProjectPointerOntoScreenLine(
      {0.0f, 0.0f, 0.0f}, {10.0f, 20.0f, 30.0f}, {100.0f, 200.0f},
      {300.0f, 200.0f}, {200.0f, 250.0f});
  ok &= Expect(middle.has_value(), "visible line should accept projection");
  ok &= Expect(middle && std::fabs((*middle)[0] - 5.0f) < 0.001f &&
                   std::fabs((*middle)[1] - 10.0f) < 0.001f &&
                   std::fabs((*middle)[2] - 15.0f) < 0.001f,
               "screen midpoint should map to world midpoint");
  const auto clamped = viewer3d::ProjectPointerOntoScreenLine(
      {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {100.0f, 200.0f},
      {300.0f, 200.0f}, {500.0f, 200.0f});
  ok &= Expect(clamped && (*clamped)[0] == 10.0f,
               "pointer projection should clamp to line endpoint");
  ok &= Expect(!viewer3d::ProjectPointerOntoScreenLine(
                    {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {100.0f, 200.0f},
                    {100.0f, 200.0f}, {100.0f, 200.0f})
                    .has_value(),
               "edge-on line should reject ambiguous projection");
  return ok ? 0 : 1;
}
