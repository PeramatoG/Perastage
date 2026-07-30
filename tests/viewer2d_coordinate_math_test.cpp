#include "viewer2d_coordinate_math.h"

#include <array>
#include <cassert>
#include <cmath>

namespace {

// Verifies two floating-point coordinates agree within interaction tolerance.
void ExpectNear(float actual, float expected) {
  assert(std::abs(actual - expected) < 0.0002f);
}

// Verifies both directions of the coordinate mapping for one view and state.
void CheckRoundTrip(Viewer2DView view, float zoom, float panX, float panY,
                    float framebufferScale) {
  const viewer2d::CoordinateTransform transform{
      static_cast<int>(800 * framebufferScale),
      static_cast<int>(600 * framebufferScale),
      zoom,
      panX,
      panY,
      view};
  const std::array<float, 3> world{2.5f, -1.75f, 4.25f};
  const auto framebuffer = viewer2d::WorldToFramebuffer(world, transform);
  assert(framebuffer);
  const auto restored = viewer2d::FramebufferToWorld(*framebuffer, transform);
  assert(restored);
  const auto secondFramebuffer =
      viewer2d::WorldToFramebuffer(*restored, transform);
  assert(secondFramebuffer);
  ExpectNear((*secondFramebuffer)[0], (*framebuffer)[0]);
  ExpectNear((*secondFramebuffer)[1], (*framebuffer)[1]);

  if (view == Viewer2DView::Top || view == Viewer2DView::Bottom) {
    ExpectNear((*restored)[0], world[0]);
    ExpectNear((*restored)[1], world[1]);
  } else if (view == Viewer2DView::Front) {
    ExpectNear((*restored)[0], world[0]);
    ExpectNear((*restored)[2], world[2]);
  } else {
    ExpectNear((*restored)[1], world[1]);
    ExpectNear((*restored)[2], world[2]);
  }
}

} // namespace

// Exercises every 2D view under default, zoomed, panned, and scaled states.
int main() {
  for (Viewer2DView view : {Viewer2DView::Top, Viewer2DView::Bottom,
                            Viewer2DView::Front, Viewer2DView::Side}) {
    CheckRoundTrip(view, 1.0f, 0.0f, 0.0f, 1.0f);
    CheckRoundTrip(view, 2.4f, 0.0f, 0.0f, 1.0f);
    CheckRoundTrip(view, 1.0f, 37.0f, -19.0f, 1.0f);
    CheckRoundTrip(view, 0.65f, -43.0f, 28.0f, 1.0f);
    CheckRoundTrip(view, 1.7f, 12.0f, -31.0f, 2.0f);
  }
  return 0;
}
