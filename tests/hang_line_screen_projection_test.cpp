#include "screen_line_projection.h"

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
  const auto middle = viewer_common::ProjectPointerOntoScreenLine(
      {0.0f, 0.0f, 0.0f}, {10.0f, 20.0f, 30.0f}, {100.0f, 200.0f},
      {300.0f, 200.0f}, {200.0f, 250.0f});
  ok &= Expect(middle.has_value(), "visible line should accept projection");
  ok &= Expect(middle && std::fabs((*middle)[0] - 5.0f) < 0.001f &&
                   std::fabs((*middle)[1] - 10.0f) < 0.001f &&
                   std::fabs((*middle)[2] - 15.0f) < 0.001f,
               "screen midpoint should map to world midpoint");
  const auto clamped = viewer_common::ProjectPointerOntoScreenLine(
      {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {100.0f, 200.0f},
      {300.0f, 200.0f}, {500.0f, 200.0f});
  ok &= Expect(clamped && (*clamped)[0] == 10.0f,
               "pointer projection should clamp to line endpoint");
  const auto vertical = viewer_common::ProjectPointerOntoScreenLine(
      {0.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f}, {100.0f, 100.0f},
      {110.0f, 300.0f}, {20.0f, 200.0f});
  ok &= Expect(vertical && std::fabs((*vertical)[1] - 5.0f) < 0.001f,
               "vertical line should follow the pointer y coordinate");
  const auto perspective = viewer_common::ProjectPointerOntoPerspectiveLine(
      {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {150.0f, 200.0f},
      [](const std::array<float, 3> &world) {
        return std::optional<std::array<float, 2>>{
            {100.0f + 200.0f * world[0] / (world[0] + 10.0f), 200.0f}};
      });
  ok &= Expect(perspective &&
                   std::fabs((*perspective)[0] - 3.333333f) < 0.001f,
               "perspective line should reproject onto the pointer coordinate");
  ok &= Expect(!viewer_common::ProjectPointerOntoScreenLine(
                    {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {100.0f, 200.0f},
                    {100.0f, 200.0f}, {100.0f, 200.0f})
                    .has_value(),
               "edge-on line should reject ambiguous projection");
  return ok ? 0 : 1;
}
