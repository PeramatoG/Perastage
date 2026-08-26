#pragma once

#include <array>
#include <optional>
#include <vector>

namespace truss_screen_snap {

constexpr double kDefaultTrussScreenSnapApertureLogicalPx = 16.0;
constexpr double kDefaultFixturePathScreenSnapApertureLogicalPx = 16.0;

struct ProjectionSnapshot {
  std::array<double, 16> modelView{};
  std::array<double, 16> projection{};
  std::array<int, 4> viewport{};
  double contentScale = 1.0;
};

struct ProjectedPoint {
  double logicalX = 0.0;
  double logicalY = 0.0;
  double depth = 0.0;
  double clipW = 1.0;
};

struct ProjectedPolylinePoint {
  std::array<float, 3> worldPointMm{};
  float pathParameter = 0.0f;
  double screenDistanceLogicalPx = 0.0;
  double depthDifference = 0.0;
  double worldDistanceMm = 0.0;
};

// Projects a scene-millimetre point using an immutable OpenGL-style snapshot.
std::optional<ProjectedPoint> Project(const ProjectionSnapshot &snapshot,
                                      const std::array<float, 3> &worldPointMm);

// Finds the screen-nearest point and reconstructs its perspective-correct world
// position.
std::optional<ProjectedPolylinePoint> ClosestPointOnProjectedPolyline(
    const ProjectionSnapshot &snapshot,
    const std::vector<std::array<float, 3>> &worldPointsMm,
    const std::array<float, 3> &queryWorldPointMm);

} // namespace truss_screen_snap
