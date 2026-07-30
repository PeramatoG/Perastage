#include "pixel_coordinate_math.h"
#include "viewer2d_coordinate_math.h"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>

namespace {

// Verifies two floating-point coordinates agree within interaction tolerance.
void ExpectNear(float actual, float expected) {
  assert(std::abs(actual - expected) < 0.0002f);
}

// Verifies explicit camera-basis directions rather than only inverse parity.
void CheckExpectedDirections(Viewer2DView view, int horizontalAxis,
                             float horizontalSign, int verticalAxis,
                             float verticalSign) {
  const viewer2d::CoordinateTransform transform{800,  600,  1.0f,
                                                0.0f, 0.0f, view};
  const std::array<float, 3> origin{0.0f, 0.0f, 0.0f};
  auto horizontal = origin;
  auto vertical = origin;
  horizontal[horizontalAxis] = 1.0f;
  vertical[verticalAxis] = 1.0f;
  const auto center = viewer2d::WorldToFramebuffer(origin, transform);
  const auto right = viewer2d::WorldToFramebuffer(horizontal, transform);
  const auto up = viewer2d::WorldToFramebuffer(vertical, transform);
  assert(center && right && up);
  ExpectNear((*right)[0] - (*center)[0], 25.0f * horizontalSign);
  ExpectNear((*up)[1] - (*center)[1], -25.0f * verticalSign);

  const auto drag = viewer2d::ScreenDeltaToWorld(2.0f, 3.0f, view);
  ExpectNear(drag[horizontalAxis], 2.0f * horizontalSign);
  ExpectNear(drag[verticalAxis], 3.0f * verticalSign);
}

// Verifies projection, unprojection, and bounds under one complete view state.
void CheckState(Viewer2DView view, float zoom, float panX, float panY,
                int width, int height) {
  const viewer2d::CoordinateTransform transform{width, height, zoom,
                                                panX,  panY,   view};
  const std::array<float, 3> world{2.5f, -1.75f, 4.25f};
  const auto framebuffer = viewer2d::WorldToFramebuffer(world, transform);
  const auto restored =
      framebuffer ? viewer2d::FramebufferToWorld(*framebuffer, transform)
                  : std::nullopt;
  assert(framebuffer && restored);
  const auto projectedAgain =
      viewer2d::WorldToFramebuffer(*restored, transform);
  assert(projectedAgain);
  ExpectNear((*projectedAgain)[0], (*framebuffer)[0]);
  ExpectNear((*projectedAgain)[1], (*framebuffer)[1]);

  const auto bounds = viewer2d::ComputeOrthographicBounds(transform);
  assert(bounds);
  const auto lowerLeft = viewer2d::FramebufferToWorld(
      {0.0f, static_cast<float>(height)}, transform);
  const auto upperRight = viewer2d::FramebufferToWorld(
      {static_cast<float>(width), 0.0f}, transform);
  assert(lowerLeft && upperRight);
  const auto basis = viewer2d::GetViewBasis(view);
  ExpectNear((*lowerLeft)[basis.horizontalAxis] * basis.horizontalSign,
             bounds->left);
  ExpectNear((*upperRight)[basis.verticalAxis] * basis.verticalSign,
             bounds->top);
}

// Verifies logical/framebuffer conversion, rounding, and invalid scales.
void CheckPixelConversion() {
  for (double scale : {1.0, 1.25, 2.0}) {
    const auto framebuffer =
        pixel_coordinates::LogicalToFramebuffer({17.0, 23.0}, scale);
    assert(framebuffer);
    const auto logical = pixel_coordinates::FramebufferToLogical(
        {static_cast<double>((*framebuffer)[0]),
         static_cast<double>((*framebuffer)[1])},
        scale);
    assert(logical);
    assert(std::abs((*logical)[0] - 17) <= 1);
    assert(std::abs((*logical)[1] - 23) <= 1);
  }
  assert((pixel_coordinates::LogicalToFramebuffer({1.0, 1.0}, 1.5) ==
          std::optional<std::array<int, 2>>{{2, 2}}));
  for (double scale : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
    assert(!pixel_coordinates::LogicalToFramebuffer({1.0, 1.0}, scale));
    assert(!pixel_coordinates::FramebufferToLogical({1.0, 1.0}, scale));
  }
  assert(!pixel_coordinates::LogicalToFramebuffer(
      {std::numeric_limits<double>::max(), 1.0}, 2.0));
  assert(!pixel_coordinates::FramebufferToLogical(
      {std::numeric_limits<double>::max(), 1.0}, 0.5));
  const auto delta = pixel_coordinates::IncrementalFramebufferDelta(
      {15.0, 18.0}, {10.0, 10.0}, 1.5);
  assert((delta == std::optional<std::array<int, 2>>{{8, 12}}));
  for (double scale : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
    assert(!pixel_coordinates::IncrementalFramebufferDelta(
        {15.0, 18.0}, {10.0, 10.0}, scale));
  }
  assert(!pixel_coordinates::IncrementalFramebufferDelta(
      {std::numeric_limits<double>::max(), 1.0}, {0.0, 0.0}, 2.0));
}

} // namespace

// Exercises the authoritative basis and pixel contract for every 2D view.
int main() {
  CheckExpectedDirections(Viewer2DView::Top, 0, 1.0f, 1, 1.0f);
  CheckExpectedDirections(Viewer2DView::Bottom, 0, -1.0f, 1, 1.0f);
  CheckExpectedDirections(Viewer2DView::Front, 0, 1.0f, 2, 1.0f);
  CheckExpectedDirections(Viewer2DView::Side, 1, -1.0f, 2, 1.0f);
  for (Viewer2DView view : {Viewer2DView::Top, Viewer2DView::Bottom,
                            Viewer2DView::Front, Viewer2DView::Side}) {
    CheckState(view, 1.0f, 0.0f, 0.0f, 800, 600);
    CheckState(view, 2.4f, 0.0f, 0.0f, 800, 600);
    CheckState(view, 1.0f, 37.0f, -19.0f, 800, 600);
    CheckState(view, 0.65f, -43.0f, 28.0f, 1000, 750);
    CheckState(view, 1.7f, 12.0f, -31.0f, 1600, 1200);
  }
  CheckPixelConversion();
  return 0;
}
