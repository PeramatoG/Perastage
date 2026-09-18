#include "layout_viewer_viewport_state.h"

#include <cassert>
#include <cmath>

namespace {
using gui::layoutviewport::LayoutViewerViewportState;
using gui::layoutviewport::Rect;

// Compares floating-point zoom values with test precision.
bool Near(double lhs, double rhs) { return std::abs(lhs - rhs) < 1e-9; }

// Verifies zoom limits, fitting, and pan accumulation.
void TestStateAndFit() {
  LayoutViewerViewportState state;
  assert(Near(state.Zoom(), 1.0));
  assert(state.PanOffset().x == 0 && state.PanOffset().y == 0);
  state.SetZoom(0.01);
  assert(Near(state.Zoom(), 0.25));
  state.SetZoom(20.0);
  assert(Near(state.Zoom(), 10.0));

  state.Fit({1000, 500}, 800.0, 400.0);
  assert(Near(state.Zoom(), 1.15));
  state.Fit({500, 1000}, 400.0, 800.0);
  assert(Near(state.Zoom(), 1.15));
  state.Fit({500, 500}, 400.0, 400.0);
  assert(Near(state.Zoom(), 1.15));
  assert(state.PanOffset().x == 0 && state.PanOffset().y == 0);
  state.ApplyPan({10, -5});
  state.ApplyPan({-3, 12});
  assert(state.PanOffset().x == 7 && state.PanOffset().y == 7);
  state.Fit({0, 500}, 400.0, 400.0);
  assert(Near(state.Zoom(), 1.0));
  state.Fit({500, 500}, 0.0, 400.0);
  assert(Near(state.Zoom(), 1.0));
}

// Verifies pointer anchoring, multi-step scaling, and wheel clamps.
void TestWheelZoom() {
  LayoutViewerViewportState state;
  assert(state.ApplyWheelZoom(120, 120, {500, 500}, {1000, 1000}, 10.0));
  assert(Near(state.Zoom(), 1.1));
  assert(state.PanOffset().x == 0 && state.PanOffset().y == 0);
  assert(state.ApplyWheelZoom(-120, 120, {700, 300}, {1000, 1000}, 10.0));
  assert(Near(state.Zoom(), 1.0));
  assert(state.PanOffset().x == 19 && state.PanOffset().y == -19);
  assert(state.ApplyWheelZoom(240, 120, {500, 500}, {1000, 1000}, 10.0));
  assert(Near(state.Zoom(), 1.21));
  state.SetZoom(10.0);
  assert(!state.ApplyWheelZoom(120, 120, {0, 0}, {1000, 1000}, 10.0));
  state.SetZoom(0.25);
  assert(!state.ApplyWheelZoom(-120, 120, {0, 0}, {1000, 1000}, 10.0));
  state.SetZoom(2.0);
  assert(!state.ApplyWheelZoom(120, 120, {0, 0}, {1000, 1000}, 2.0));
  assert(!state.ApplyWheelZoom(0, 120, {0, 0}, {1000, 1000}, 10.0));
}

// Verifies page centering, pan placement, zoom, and frame mapping.
void TestGeometry() {
  LayoutViewerViewportState state;
  Rect page = state.PageRect({1000, 800}, 600.0, 400.0);
  assert(page.x == 200 && page.y == 200 && page.width == 600 &&
         page.height == 400);
  state.ApplyPan({10, -20});
  page = state.PageRect({1000, 800}, 600.0, 400.0);
  assert(page.x == 210 && page.y == 180);
  state.SetZoom(2.0);
  page = state.PageRect({1000, 800}, 600.0, 400.0);
  assert(page.x == -90 && page.y == -20 && page.width == 1200);
  Rect frame;
  assert(state.FrameRect({1000, 800}, 600.0, 400.0,
                         {10.0, 20.0, 100.0, 50.0}, frame));
  assert(frame.x == -70 && frame.y == 20 && frame.width == 200 &&
         frame.height == 100);
  assert(!state.FrameRect({1000, 800}, 600.0, 400.0,
                          {0.0, 0.0, 0.0, 10.0}, frame));
}

// Verifies automatic-fit request readiness and one-shot consumption.
void TestAutomaticFit() {
  LayoutViewerViewportState state;
  state.RequestAutomaticFit();
  assert(state.HasPendingAutomaticFit());
  assert(!state.ConsumeAutomaticFitIfReady({99, 200}, true));
  assert(!state.ConsumeAutomaticFitIfReady({200, 200}, false));
  assert(state.ConsumeAutomaticFitIfReady({200, 200}, true));
  assert(!state.HasPendingAutomaticFit());
  assert(!state.ConsumeAutomaticFitIfReady({200, 200}, true));
  state.RequestAutomaticFit();
  state.CancelAutomaticFit();
  assert(!state.HasPendingAutomaticFit());
}

// Verifies dimension, pixel/byte, and layout-wide safe zoom limits.
void TestSafeZoom() {
  using gui::layoutviewport::GetLayoutSafeMaxZoom;
  using gui::layoutviewport::GetMaxZoomForFrame;
  assert(Near(GetMaxZoomForFrame({100, 100}), 10.0));
  assert(GetMaxZoomForFrame({9000, 10}) < 1.0);
  assert(GetMaxZoomForFrame({4000, 4000}) <= 1.024000001);
  const double safe = GetLayoutSafeMaxZoom({{100, 100}, {9000, 10}});
  assert(Near(safe, GetMaxZoomForFrame({9000, 10})));
}
} // namespace

// Runs deterministic viewport policy checks without a GUI runtime.
int main() {
  TestStateAndFit();
  TestWheelZoom();
  TestGeometry();
  TestAutomaticFit();
  TestSafeZoom();
  return 0;
}
