#include "../viewer2d/render/viewer2d_render_frame_plan.h"
#include "../viewer2d/viewer2d_ruler_overlay.h"

#include <cassert>
#include <cmath>

namespace {

// Compares floating-point geometry values with a deterministic tolerance.
bool Near(float lhs, float rhs) { return std::fabs(lhs - rhs) < 0.001f; }

// Verifies configuration, interaction, capture, and override decisions.
void TestRenderFramePlan() {
  viewer2d::render::RenderFramePlanInput input;
  input.configuredDarkMode = false;
  input.configuredShowGrid = true;
  input.configuredShowRuler = false;
  input.selectionEnabled = true;
  input.pauseHeavyTasks = true;
  input.expensiveVisualInteractionActive = true;
  input.captureRequested = true;
  input.captureIncludeGrid = false;
  input.useSimplifiedFootprints = true;
  input.overrides.darkMode = true;
  input.overrides.showGrid = false;
  input.overrides.showRuler = true;
  input.overrides.drawFixtureLabels = false;
  input.overrides.forceBottomViewForTopFixtures = true;
  input.overrides.symbolCaptureRenderProfile = true;
  input.overrides.symbolCaptureIncludeCoplanarEdges = false;

  const auto plan = viewer2d::render::BuildRenderFramePlan(input);
  assert(plan.darkMode);
  assert(!plan.showGrid);
  assert(plan.showRuler);
  assert(!plan.drawFixtureLabels);
  assert(plan.skipResourceSynchronization);
  assert(plan.interactiveLabelMode);
  assert(plan.captureActive);
  assert(!plan.captureIncludeGrid);
  assert(plan.useSimplifiedFootprints);
  assert(plan.forceBottomViewForTopFixtures == true);
  assert(plan.symbolCaptureRenderProfile == true);
  assert(plan.symbolCaptureIncludeCoplanarEdges == false);

  input.selectionEnabled = false;
  const auto disabledSelectionPlan =
      viewer2d::render::BuildRenderFramePlan(input);
  assert(!disabledSelectionPlan.skipResourceSynchronization);
  assert(!disabledSelectionPlan.interactiveLabelMode);
}

// Verifies that every active view and all ruler fields survive construction.
void TestRulerStateConstruction() {
  for (const auto view : {Viewer2DView::Top, Viewer2DView::Bottom,
                          Viewer2DView::Front, Viewer2DView::Side}) {
    const auto state =
        viewer2d::render::BuildRulerOverlayViewState({1920,
                                                      1080,
                                                      2.5f,
                                                      12.0f,
                                                      -8.0f,
                                                      0.25f,
                                                      1.0f,
                                                      2.0f,
                                                      3.0f,
                                                      4.0f,
                                                      {0.1f, 0.2f, 0.3f, 1.0f},
                                                      {0.4f, 0.5f, 0.6f, 1.0f},
                                                      {0.7f, 0.8f, 0.9f, 1.0f},
                                                      true,
                                                      view});
    assert(state.width == 1920 && state.height == 1080);
    assert(Near(state.zoom, 2.5f));
    assert(Near(state.offsetPixelsX, 12.0f));
    assert(Near(state.offsetPixelsY, -8.0f));
    assert(Near(state.smallTickMeters, 0.25f));
    assert(Near(state.largeTickMeters, 1.0f));
    assert(Near(state.xRulerPositionMeters, 2.0f));
    assert(Near(state.yRulerPositionMeters, 3.0f));
    assert(Near(state.zRulerPositionMeters, 4.0f));
    assert(Near(state.xRulerColor.r, 0.1f));
    assert(Near(state.yRulerColor.g, 0.5f));
    assert(Near(state.zRulerColor.b, 0.9f));
    assert(state.useImperialUnits);
    assert(state.view == view);
  }
}

// Verifies fit, aspect, centering, scaling, and invalid-input behavior.
void TestLayoutEditGeometry() {
  const auto wide = viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {1000.0f, 500.0f, 2.0f, 1.0f, std::nullopt, std::nullopt});
  assert(wide && Near(wide->width, 800.0f) && Near(wide->height, 400.0f));
  assert(Near(wide->left, 100.0f) && Near(wide->bottom, 50.0f));

  const auto tall = viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {500.0f, 1000.0f, 0.5f, 1.0f, std::nullopt, std::nullopt});
  assert(tall && Near(tall->width, 400.0f) && Near(tall->height, 800.0f));

  const auto squareLandscape =
      viewer2d::render::ComputeLayoutEditOverlayGeometry(
          {600.0f, 600.0f, 2.0f, 0.5f, std::nullopt, std::nullopt});
  assert(squareLandscape && Near(squareLandscape->width, 240.0f));
  assert(Near(squareLandscape->height, 120.0f));
  assert(Near(squareLandscape->left, 180.0f));
  assert(Near(squareLandscape->bottom, 240.0f));

  const auto fixedBase = viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {800.0f, 600.0f, 1.0f, 2.0f, 100.0f, 50.0f});
  assert(fixedBase && Near(fixedBase->width, 200.0f));
  assert(Near(fixedBase->height, 100.0f));
  assert(Near(fixedBase->left, 300.0f));
  assert(Near(fixedBase->bottom, 250.0f));

  assert(!viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {0.0f, 600.0f, 1.0f, 1.0f, std::nullopt, std::nullopt}));
  assert(!viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {800.0f, 600.0f, 0.0f, 1.0f, std::nullopt, std::nullopt}));
  assert(!viewer2d::render::ComputeLayoutEditOverlayGeometry(
      {800.0f, 600.0f, 1.0f, 0.0f, std::nullopt, std::nullopt}));
}

} // namespace

// Runs deterministic render-planning and overlay-geometry checks.
int main() {
  TestRenderFramePlan();
  TestRulerStateConstruction();
  TestLayoutEditGeometry();
  return 0;
}
