#pragma once

#include "../../viewer3d/viewer3d_types.h"
#include "../canvas2d.h"

#include <optional>

namespace viewer2d {
struct RulerOverlayViewState;
}

namespace viewer2d::render {

struct RenderDecisionOverrides {
  std::optional<bool> darkMode;
  std::optional<bool> showGrid;
  std::optional<bool> showRuler;
  std::optional<bool> drawFixtureLabels;
  std::optional<bool> forceBottomViewForTopFixtures;
  std::optional<bool> symbolCaptureRenderProfile;
  std::optional<bool> symbolCaptureIncludeCoplanarEdges;
};

struct RenderFramePlanInput {
  bool configuredDarkMode = false;
  bool configuredShowGrid = true;
  bool configuredShowRuler = false;
  bool selectionEnabled = true;
  bool pauseHeavyTasks = false;
  bool expensiveVisualInteractionActive = false;
  bool captureRequested = false;
  bool captureIncludeGrid = true;
  bool useSimplifiedFootprints = false;
  RenderDecisionOverrides overrides;
};

struct RenderFramePlan {
  bool darkMode = false;
  bool showGrid = true;
  bool showRuler = false;
  bool drawFixtureLabels = true;
  bool skipResourceSynchronization = false;
  bool interactiveLabelMode = false;
  bool captureActive = false;
  bool captureIncludeGrid = true;
  bool useSimplifiedFootprints = false;
  std::optional<bool> forceBottomViewForTopFixtures;
  std::optional<bool> symbolCaptureRenderProfile;
  std::optional<bool> symbolCaptureIncludeCoplanarEdges;
};

struct RulerFrameInput {
  int width = 0;
  int height = 0;
  float zoom = 1.0f;
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  float smallTickMeters = 0.1f;
  float largeTickMeters = 0.2f;
  float xRulerPositionMeters = 0.0f;
  float yRulerPositionMeters = 0.0f;
  float zRulerPositionMeters = 0.0f;
  CanvasColor xRulerColor{0.0f, 0.0f, 0.0f, 1.0f};
  CanvasColor yRulerColor{0.0f, 0.0f, 0.0f, 1.0f};
  CanvasColor zRulerColor{0.0f, 0.0f, 0.0f, 1.0f};
  bool useImperialUnits = false;
  Viewer2DView view = Viewer2DView::Top;
};

struct LayoutEditOverlayInput {
  float viewportWidth = 0.0f;
  float viewportHeight = 0.0f;
  float aspectRatio = 0.0f;
  float scale = 1.0f;
  std::optional<float> baseWidth;
  std::optional<float> baseHeight;
};

struct LayoutEditOverlayGeometry {
  float baseWidth = 0.0f;
  float baseHeight = 0.0f;
  float left = 0.0f;
  float bottom = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

// Resolves stable render decisions from configuration and temporary overrides.
RenderFramePlan BuildRenderFramePlan(const RenderFramePlanInput &input);

// Constructs the renderer-neutral ruler state used by all ruler emitters.
RulerOverlayViewState BuildRulerOverlayViewState(const RulerFrameInput &input);

// Fits and centers the layout-edit rectangle in framebuffer coordinates.
std::optional<LayoutEditOverlayGeometry>
ComputeLayoutEditOverlayGeometry(const LayoutEditOverlayInput &input);

} // namespace viewer2d::render
