#include "../viewer2d/interaction/viewer2d_navigation_policy.h"
#include "../viewer2d/viewer2d_coordinate_math.h"

#include <array>
#include <cassert>
#include <cmath>

using namespace viewer2d::interaction;

// Reports whether two floating-point values are sufficiently close.
static bool Near(float lhs, float rhs) {
  return std::fabs(lhs - rhs) < 0.0001f;
}

// Checks how a fixed world point moves through the viewport projection.
static void VerifyProjectedDragPan() {
  constexpr Viewer2DView views[] = {Viewer2DView::Top, Viewer2DView::Front,
                                    Viewer2DView::Side};
  constexpr float zoomLevels[] = {0.5f, 1.0f, 2.0f, 4.0f};
  constexpr std::array<int, 2> drags[] = {
      {0, -12}, {0, 8}, {-10, 0}, {6, 0}, {6, -4}, {-2, 8}, {0, 0}};
  constexpr std::array<float, 3> worldPoint{1.0f, -0.5f, 0.75f};

  for (const auto view : views) {
    for (const float zoom : zoomLevels) {
      const NavigationViewState initial{1.0f, 2.0f, zoom};
      const auto before = viewer2d::WorldToFramebuffer(
          worldPoint, {800, 600, zoom, initial.offsetX, initial.offsetY, view});
      assert(before);

      for (const auto &[dx, dy] : drags) {
        const auto result =
            Viewer2DNavigationPolicy::ApplyDragPan(initial, dx, dy);
        assert(result.handled == (dx != 0 || dy != 0));
        assert(Near(result.view.zoom, zoom));
        const auto after = viewer2d::WorldToFramebuffer(
            worldPoint, {800, 600, result.view.zoom, result.view.offsetX,
                         result.view.offsetY, view});
        assert(after);
        assert(Near((*after)[0] - (*before)[0], static_cast<float>(dx)));
        assert(Near((*after)[1] - (*before)[1], static_cast<float>(dy)));
      }
    }
  }
}

// Verifies drag, wheel, and keyboard navigation decisions.
int main() {
  VerifyProjectedDragPan();

  auto result =
      Viewer2DNavigationPolicy::ApplyDragPan({1.0f, 2.0f, 2.0f}, 6, -4);
  assert(result.handled && Near(result.view.offsetX, 4.0f));
  assert(Near(result.view.offsetY, 4.0f));
  result = Viewer2DNavigationPolicy::ApplyDragPan(result.view, -2, 8);
  assert(Near(result.view.offsetX, 3.0f) && Near(result.view.offsetY, 0.0f));

  result = Viewer2DNavigationPolicy::ApplyWheelZoom({0.0f, 0.0f, 1.0f}, 1.0f);
  assert(Near(result.view.zoom, 1.1f));
  result = Viewer2DNavigationPolicy::ApplyWheelZoom(result.view, -1.0f);
  assert(Near(result.view.zoom, 1.0f));
  result = Viewer2DNavigationPolicy::ApplyWheelZoom(result.view, 3.0f);
  assert(Near(result.view.zoom, 1.331f));
  result =
      Viewer2DNavigationPolicy::ApplyWheelZoom({0.0f, 0.0f, 0.11f}, -20.0f);
  assert(Near(result.view.zoom, 0.1f));

  const NavigationViewState view{0.0f, 0.0f, 2.0f};
  assert(Near(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Left, false)
          .view.offsetX,
      5.0f));
  assert(Near(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Right, false)
          .view.offsetX,
      -5.0f));
  assert(Near(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Up, false)
          .view.offsetY,
      -5.0f));
  assert(Near(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Down, false)
          .view.offsetY,
      5.0f));
  assert(Near(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Left, true)
          .view.zoom,
      2.2f));
  assert(Near(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Up, true)
          .view.zoom,
      2.2f));
  assert(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Right, true)
          .view.zoom < 2.0f);
  assert(
      Viewer2DNavigationPolicy::ApplyKeyboard(view, NavigationKey::Down, true)
          .view.zoom < 2.0f);
  assert(!Viewer2DNavigationPolicy::ApplyKeyboard(
              view, NavigationKey::Unsupported, false)
              .handled);
  assert(Near(Viewer2DNavigationPolicy::ApplyKeyboard(
                  {0.0f, 0.0f, 0.1f}, NavigationKey::Right, true)
                  .view.zoom,
              0.1f));
  return 0;
}
