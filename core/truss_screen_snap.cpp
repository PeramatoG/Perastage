#include "truss_screen_snap.h"

#include <cmath>

namespace truss_screen_snap {
namespace {

// Multiplies an OpenGL column-major matrix by a homogeneous vector.
std::array<double, 4> Multiply(const std::array<double, 16> &matrix,
                               const std::array<double, 4> &vector) {
  std::array<double, 4> result{};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column)
      result[row] += matrix[column * 4 + row] * vector[column];
  }
  return result;
}

} // namespace

// Projects a scene-millimetre point using an immutable OpenGL-style snapshot.
std::optional<ProjectedPoint>
Project(const ProjectionSnapshot &snapshot,
        const std::array<float, 3> &worldPointMm) {
  if (!std::isfinite(snapshot.contentScale) || snapshot.contentScale <= 0.0 ||
      snapshot.viewport[2] <= 0 || snapshot.viewport[3] <= 0)
    return std::nullopt;
  const std::array<double, 4> world{worldPointMm[0] / 1000.0,
                                    worldPointMm[1] / 1000.0,
                                    worldPointMm[2] / 1000.0, 1.0};
  const auto eye = Multiply(snapshot.modelView, world);
  const auto clip = Multiply(snapshot.projection, eye);
  if (!std::isfinite(clip[0]) || !std::isfinite(clip[1]) ||
      !std::isfinite(clip[2]) || !std::isfinite(clip[3]) || clip[3] <= 0.0)
    return std::nullopt;
  const double x = clip[0] / clip[3];
  const double y = clip[1] / clip[3];
  const double z = clip[2] / clip[3];
  if (x < -1.0 || x > 1.0 || y < -1.0 || y > 1.0 || z < -1.0 || z > 1.0)
    return std::nullopt;
  return ProjectedPoint{
      (snapshot.viewport[0] + (x + 1.0) * snapshot.viewport[2] * 0.5) /
          snapshot.contentScale,
      (snapshot.viewport[1] + (y + 1.0) * snapshot.viewport[3] * 0.5) /
          snapshot.contentScale,
      eye[2]};
}

} // namespace truss_screen_snap
