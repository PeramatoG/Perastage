#pragma once

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

// MVR coordinates are defined in millimeters. This constant converts
// them to meters when rendering.
static constexpr float RENDER_SCALE = 0.001f;

// Rendering options for the simplified 2D top-down view
enum class Viewer2DRenderMode {
  Wireframe = 0,
  White,
  ByFixtureType,
  ByLayer
};

// Available orientations for the 2D viewer
enum class Viewer2DView {
  Top = 0,
  Front,
  Side,
  Bottom
};

enum class Viewer3DItemType : int { Fixture, Truss, SceneObject };

struct Viewer3DVisibleSet {
  std::vector<std::string> fixtureUuids;
  std::vector<std::string> trussUuids;
  std::vector<std::string> objectUuids;

  bool Empty() const {
    return fixtureUuids.empty() && trussUuids.empty() && objectUuids.empty();
  }
};

struct Viewer3DViewFrustumSnapshot {
  int viewport[4] = {0, 0, 0, 0};
  double model[16] = {0.0};
  double projection[16] = {0.0};
};

struct RenderFrameContext {
  Viewer2DRenderMode mode = Viewer2DRenderMode::White;
  Viewer2DView view = Viewer2DView::Top;
  bool wireframe = false;
  bool showGrid = true;
  int gridStyle = 0;
  float gridR = 0.35f;
  float gridG = 0.35f;
  float gridB = 0.35f;
  bool gridOnTop = false;
  bool is2DViewer = false;

  bool useLighting = true;
  bool drawGridBeforeScene = false;
  bool drawGridAfterScene = false;
  bool useFrustumCulling = false;
  float minCullingPixels = 0.0f;

  bool fastInteractionMode = false;
  bool skipOptionalWork = false;
  bool skipCapture = false;
  bool skipOutlinesForCurrentFrame = false;

  bool colorByFixtureType = false;
  bool colorByLayer = false;

  std::unordered_set<std::string> hiddenLayers;
};
