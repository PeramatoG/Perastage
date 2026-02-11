#pragma once

#include <array>
#include <string>
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

enum class ViewerItemType : int { Fixture, Truss, SceneObject };

struct ViewerVisibleSet {
  std::vector<std::string> fixtureUuids;
  std::vector<std::string> trussUuids;
  std::vector<std::string> objectUuids;

  bool Empty() const {
    return fixtureUuids.empty() && trussUuids.empty() && objectUuids.empty();
  }
};

struct ViewerViewFrustumSnapshot {
  int viewport[4] = {0, 0, 0, 0};
  double model[16] = {0.0};
  double projection[16] = {0.0};
};
