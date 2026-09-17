#include "viewer2d_render_frame_plan.h"

#include "../viewer2d_ruler_overlay.h"

#include <algorithm>
#include <cmath>

namespace viewer2d::render {
namespace {

// Applies a temporary boolean override when one is present.
bool ResolveOverride(bool configured,
                     const std::optional<bool> &overrideValue) {
  return overrideValue.value_or(configured);
}

} // namespace

// Resolves stable render decisions from configuration and temporary overrides.
RenderFramePlan BuildRenderFramePlan(const RenderFramePlanInput &input) {
  RenderFramePlan plan;
  plan.darkMode =
      ResolveOverride(input.configuredDarkMode, input.overrides.darkMode);
  plan.showGrid =
      ResolveOverride(input.configuredShowGrid, input.overrides.showGrid);
  plan.showRuler =
      ResolveOverride(input.configuredShowRuler, input.overrides.showRuler);
  plan.drawFixtureLabels =
      ResolveOverride(true, input.overrides.drawFixtureLabels);
  plan.skipResourceSynchronization =
      input.selectionEnabled && input.pauseHeavyTasks;
  plan.interactiveLabelMode =
      input.selectionEnabled && input.expensiveVisualInteractionActive;
  plan.captureActive = input.captureRequested;
  plan.captureIncludeGrid = input.captureIncludeGrid;
  plan.useSimplifiedFootprints = input.useSimplifiedFootprints;
  plan.forceBottomViewForTopFixtures =
      input.overrides.forceBottomViewForTopFixtures;
  plan.symbolCaptureRenderProfile = input.overrides.symbolCaptureRenderProfile;
  plan.symbolCaptureIncludeCoplanarEdges =
      input.overrides.symbolCaptureIncludeCoplanarEdges;
  return plan;
}

// Constructs the renderer-neutral ruler state used by all ruler emitters.
RulerOverlayViewState BuildRulerOverlayViewState(const RulerFrameInput &input) {
  RulerOverlayViewState state;
  state.width = input.width;
  state.height = input.height;
  state.zoom = input.zoom;
  state.offsetPixelsX = input.offsetPixelsX;
  state.offsetPixelsY = input.offsetPixelsY;
  state.smallTickMeters = input.smallTickMeters;
  state.largeTickMeters = input.largeTickMeters;
  state.xRulerPositionMeters = input.xRulerPositionMeters;
  state.yRulerPositionMeters = input.yRulerPositionMeters;
  state.zRulerPositionMeters = input.zRulerPositionMeters;
  state.xRulerColor = input.xRulerColor;
  state.yRulerColor = input.yRulerColor;
  state.zRulerColor = input.zRulerColor;
  state.useImperialUnits = input.useImperialUnits;
  state.view = input.view;
  return state;
}

// Fits and centers the layout-edit rectangle in framebuffer coordinates.
std::optional<LayoutEditOverlayGeometry>
ComputeLayoutEditOverlayGeometry(const LayoutEditOverlayInput &input) {
  if (!std::isfinite(input.viewportWidth) ||
      !std::isfinite(input.viewportHeight) ||
      !std::isfinite(input.aspectRatio) || !std::isfinite(input.scale) ||
      input.viewportWidth <= 0.0f || input.viewportHeight <= 0.0f ||
      input.aspectRatio <= 0.0f || input.scale <= 0.0f) {
    return std::nullopt;
  }

  float baseWidth = input.baseWidth.value_or(0.0f);
  float baseHeight = input.baseHeight.value_or(0.0f);
  if (baseWidth <= 0.0f || baseHeight <= 0.0f) {
    const float padding =
        std::min(input.viewportWidth, input.viewportHeight) * 0.1f;
    const float maxWidth = input.viewportWidth - padding * 2.0f;
    const float maxHeight = input.viewportHeight - padding * 2.0f;
    baseWidth = maxWidth;
    baseHeight = baseWidth / input.aspectRatio;
    if (baseHeight > maxHeight) {
      baseHeight = maxHeight;
      baseWidth = baseHeight * input.aspectRatio;
    }
  }

  baseWidth = std::round(baseWidth);
  baseHeight = std::round(baseHeight);
  if (!std::isfinite(baseWidth) || !std::isfinite(baseHeight) ||
      baseWidth <= 0.0f || baseHeight <= 0.0f) {
    return std::nullopt;
  }

  LayoutEditOverlayGeometry geometry;
  geometry.baseWidth = baseWidth;
  geometry.baseHeight = baseHeight;
  geometry.width = baseWidth * input.scale;
  geometry.height = baseHeight * input.scale;
  geometry.left = (input.viewportWidth - geometry.width) * 0.5f;
  geometry.bottom = (input.viewportHeight - geometry.height) * 0.5f;
  return geometry;
}

} // namespace viewer2d::render
