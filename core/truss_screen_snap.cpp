#include "truss_screen_snap.h"

#include <algorithm>
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
      eye[2], clip[3]};
}

// Finds the screen-nearest point and reconstructs its perspective-correct world
// position.
std::optional<ProjectedPolylinePoint> ClosestPointOnProjectedPolyline(
    const ProjectionSnapshot &snapshot,
    const std::vector<std::array<float, 3>> &worldPointsMm,
    const std::array<float, 3> &queryWorldPointMm) {
  if (worldPointsMm.size() < 2)
    return std::nullopt;
  const auto query = Project(snapshot, queryWorldPointMm);
  if (!query)
    return std::nullopt;

  std::vector<double> segmentLengths;
  double totalLength = 0.0;
  for (std::size_t index = 1; index < worldPointsMm.size(); ++index) {
    double squaredLength = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double delta =
          worldPointsMm[index][axis] - worldPointsMm[index - 1][axis];
      squaredLength += delta * delta;
    }
    segmentLengths.push_back(std::sqrt(squaredLength));
    totalLength += segmentLengths.back();
  }

  std::optional<ProjectedPolylinePoint> best;
  double precedingLength = 0.0;
  for (std::size_t index = 1; index < worldPointsMm.size(); ++index) {
    const auto start = Project(snapshot, worldPointsMm[index - 1]);
    const auto end = Project(snapshot, worldPointsMm[index]);
    if (!start || !end) {
      precedingLength += segmentLengths[index - 1];
      continue;
    }
    const double dx = end->logicalX - start->logicalX;
    const double dy = end->logicalY - start->logicalY;
    const double lengthSquared = dx * dx + dy * dy;
    const double screenParameter =
        lengthSquared > 1e-12
            ? std::clamp(((query->logicalX - start->logicalX) * dx +
                          (query->logicalY - start->logicalY) * dy) /
                             lengthSquared,
                         0.0, 1.0)
            : 0.0;
    const double denominator =
        (1.0 - screenParameter) * end->clipW + screenParameter * start->clipW;
    if (std::fabs(denominator) <= 1e-12) {
      precedingLength += segmentLengths[index - 1];
      continue;
    }
    const double worldParameter = screenParameter * start->clipW / denominator;
    std::array<float, 3> worldPoint{};
    double worldDistanceSquared = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      worldPoint[axis] = static_cast<float>(
          worldPointsMm[index - 1][axis] +
          (worldPointsMm[index][axis] - worldPointsMm[index - 1][axis]) *
              worldParameter);
      const double delta = worldPoint[axis] - queryWorldPointMm[axis];
      worldDistanceSquared += delta * delta;
    }
    const double nearestX = start->logicalX + dx * screenParameter;
    const double nearestY = start->logicalY + dy * screenParameter;
    const double screenDistance =
        std::hypot(nearestX - query->logicalX, nearestY - query->logicalY);
    const auto projectedWorldPoint = Project(snapshot, worldPoint);
    if (!projectedWorldPoint) {
      precedingLength += segmentLengths[index - 1];
      continue;
    }
    ProjectedPolylinePoint candidate{
        worldPoint,
        totalLength > 1e-12
            ? static_cast<float>((precedingLength +
                                  worldParameter * segmentLengths[index - 1]) /
                                 totalLength)
            : 0.0f,
        screenDistance, std::fabs(projectedWorldPoint->depth - query->depth),
        std::sqrt(worldDistanceSquared)};
    if (!best ||
        candidate.screenDistanceLogicalPx < best->screenDistanceLogicalPx)
      best = candidate;
    precedingLength += segmentLengths[index - 1];
  }
  return best;
}

} // namespace truss_screen_snap
