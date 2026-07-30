#pragma once

#include <array>
#include <optional>

namespace truss_screen_snap {

constexpr double kDefaultTrussScreenSnapApertureLogicalPx = 16.0;

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
};

// Projects a scene-millimetre point using an immutable OpenGL-style snapshot.
std::optional<ProjectedPoint> Project(const ProjectionSnapshot &snapshot,
                                      const std::array<float, 3> &worldPointMm);

} // namespace truss_screen_snap
