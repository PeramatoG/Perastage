#include "fixture_line_distribution.h"

#include <algorithm>
#include <cmath>

namespace fixture_line_distribution {
namespace {

using Point = std::array<float, 3>;

// Transforms a local millimeter point into world meters.
Point TransformPoint(const Matrix &transform, const Point &localMm) {
  Point result = transform.o;
  for (int component = 0; component < 3; ++component)
    result[component] += (transform.u[component] * localMm[0] +
                          transform.v[component] * localMm[1] +
                          transform.w[component] * localMm[2]) /
                         1000.0f;
  return result;
}

// Builds the longitudinal center line from truss geometry or dimensions.
std::optional<Line> BuildLine(const Truss &truss) {
  Point localStart{};
  Point localEnd{};
  if (truss.localGeometryBounds && truss.localGeometryBounds->IsValid()) {
    const auto size = truss.localGeometryBounds->SizeMm();
    const int axis = static_cast<int>(
        std::max_element(size.begin(), size.end()) - size.begin());
    localStart = truss.localGeometryBounds->CenterMm();
    localEnd = localStart;
    localStart[axis] = truss.localGeometryBounds->minMm[axis];
    localEnd[axis] = truss.localGeometryBounds->maxMm[axis];
  } else if (std::isfinite(truss.lengthMm) && truss.lengthMm > 0.0f) {
    localStart[0] = -truss.lengthMm * 0.5f;
    localEnd[0] = truss.lengthMm * 0.5f;
  } else {
    return std::nullopt;
  }
  return Line{TransformPoint(truss.transform, localStart),
              TransformPoint(truss.transform, localEnd), truss.uuid};
}

// Returns squared Euclidean distance between two points.
float DistanceSquared(const Point &lhs, const Point &rhs) {
  float result = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    const float delta = lhs[axis] - rhs[axis];
    result += delta * delta;
  }
  return result;
}

} // namespace

// Projects a world point onto a finite truss line.
Point ProjectOntoLine(const Line &line, const Point &point) {
  Point direction{};
  float lengthSquared = 0.0f;
  float along = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    direction[axis] = line.end[axis] - line.start[axis];
    lengthSquared += direction[axis] * direction[axis];
    along += (point[axis] - line.start[axis]) * direction[axis];
  }
  const float fraction = lengthSquared > 1e-10f
                             ? std::clamp(along / lengthSquared, 0.0f, 1.0f)
                             : 0.0f;
  Point projected{};
  for (int axis = 0; axis < 3; ++axis)
    projected[axis] = line.start[axis] + direction[axis] * fraction;
  return projected;
}

// Resolves the single truss line containing every selected fixture.
ResolveResult ResolveSelectedLine(const MvrScene &scene,
                                  const std::vector<std::string> &fixtureUuids,
                                  float toleranceMeters) {
  if (fixtureUuids.size() < 2)
    return {.line = std::nullopt, .error = ResolveError::TooFewFixtures};
  for (const std::string &uuid : fixtureUuids) {
    if (scene.fixtures.find(uuid) == scene.fixtures.end())
      return {.line = std::nullopt, .error = ResolveError::MissingFixture};
  }
  const float toleranceSquared = toleranceMeters * toleranceMeters;
  for (const auto &[uuid, truss] : scene.trusses) {
    const auto centerLine = BuildLine(truss);
    if (!centerLine)
      continue;
    Line line = *centerLine;
    const Point firstPosition =
        scene.fixtures.at(fixtureUuids.front()).transform.o;
    const Point firstProjection = ProjectOntoLine(line, firstPosition);
    for (int axis = 0; axis < 3; ++axis) {
      const float mountingOffset = firstPosition[axis] - firstProjection[axis];
      line.start[axis] += mountingOffset;
      line.end[axis] += mountingOffset;
    }
    bool containsAll = true;
    for (const std::string &fixtureUuid : fixtureUuids) {
      const Point position = scene.fixtures.at(fixtureUuid).transform.o;
      if (DistanceSquared(position, ProjectOntoLine(line, position)) >
          toleranceSquared) {
        containsAll = false;
        break;
      }
    }
    if (containsAll)
      return {.line = line, .error = ResolveError::None};
  }
  return {.line = std::nullopt, .error = ResolveError::NotOnSameTruss};
}

// Distributes fixtures in selection order between two positions on a line.
bool Apply(MvrScene &scene, const std::vector<std::string> &fixtureUuids,
           const Point &start, const Point &end, bool includeEndpointMargins) {
  if (fixtureUuids.size() < 2)
    return false;
  const float divisor = includeEndpointMargins
                            ? static_cast<float>(fixtureUuids.size() + 1)
                            : static_cast<float>(fixtureUuids.size() - 1);
  for (std::size_t index = 0; index < fixtureUuids.size(); ++index) {
    auto fixture = scene.fixtures.find(fixtureUuids[index]);
    if (fixture == scene.fixtures.end())
      return false;
    const float fraction = includeEndpointMargins
                               ? static_cast<float>(index + 1) / divisor
                               : static_cast<float>(index) / divisor;
    for (int axis = 0; axis < 3; ++axis)
      fixture->second.transform.o[axis] =
          start[axis] + (end[axis] - start[axis]) * fraction;
  }
  return true;
}

} // namespace fixture_line_distribution
