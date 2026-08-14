#include "fixture_line_distribution.h"

#include <algorithm>
#include <cmath>

namespace fixture_line_distribution {
namespace {

using Point = std::array<float, 3>;

// Transforms a local millimeter point into world millimeters.
Point TransformPoint(const Matrix &transform, const Point &localMm) {
  Point result = transform.o;
  for (int component = 0; component < 3; ++component)
    result[component] += transform.u[component] * localMm[0] +
                         transform.v[component] * localMm[1] +
                         transform.w[component] * localMm[2];
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

// Returns the scalar projection of a point along an unbounded line.
float ParameterAlongLine(const Line &line, const Point &point) {
  Point direction{};
  float lengthSquared = 0.0f;
  float along = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    direction[axis] = line.end[axis] - line.start[axis];
    lengthSquared += direction[axis] * direction[axis];
    along += (point[axis] - line.start[axis]) * direction[axis];
  }
  return lengthSquared > 1e-6f ? along / std::sqrt(lengthSquared) : 0.0f;
}

// Returns the closest point on the unbounded extension of a line.
Point ProjectOntoInfiniteLine(const Line &line, const Point &point) {
  Point direction{};
  float lengthSquared = 0.0f;
  float along = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    direction[axis] = line.end[axis] - line.start[axis];
    lengthSquared += direction[axis] * direction[axis];
    along += (point[axis] - line.start[axis]) * direction[axis];
  }
  const float fraction = lengthSquared > 1e-6f ? along / lengthSquared : 0.0f;
  Point projected{};
  for (int axis = 0; axis < 3; ++axis)
    projected[axis] = line.start[axis] + direction[axis] * fraction;
  return projected;
}

// Merges connected collinear truss segments into one bridge line.
std::optional<Line> BuildConnectedBridge(const std::vector<Line> &segments,
                                         std::size_t seedIndex) {
  constexpr float kParallelCosine = 0.999f;
  constexpr float kCenterlineToleranceMm = 100.0f;
  constexpr float kConnectionGapMm = 150.0f;
  const Line &seed = segments[seedIndex];
  Point seedDirection{};
  float seedLength = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    seedDirection[axis] = seed.end[axis] - seed.start[axis];
    seedLength += seedDirection[axis] * seedDirection[axis];
  }
  seedLength = std::sqrt(seedLength);
  if (seedLength <= 1e-3f)
    return std::nullopt;
  for (float &value : seedDirection)
    value /= seedLength;

  std::vector<std::pair<float, float>> intervals;
  for (const Line &segment : segments) {
    Point direction{};
    float length = 0.0f;
    for (int axis = 0; axis < 3; ++axis) {
      direction[axis] = segment.end[axis] - segment.start[axis];
      length += direction[axis] * direction[axis];
    }
    length = std::sqrt(length);
    if (length <= 1e-3f)
      continue;
    float cosine = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
      cosine += seedDirection[axis] * direction[axis] / length;
    if (std::fabs(cosine) < kParallelCosine ||
        DistanceSquared(segment.start,
                        ProjectOntoInfiniteLine(seed, segment.start)) >
            kCenterlineToleranceMm * kCenterlineToleranceMm)
      continue;
    float start = ParameterAlongLine(seed, segment.start);
    float end = ParameterAlongLine(seed, segment.end);
    if (start > end)
      std::swap(start, end);
    intervals.emplace_back(start, end);
  }
  if (intervals.empty())
    return std::nullopt;
  std::sort(intervals.begin(), intervals.end());
  std::vector<std::pair<float, float>> connected;
  connected.push_back(intervals.front());
  for (std::size_t index = 1; index < intervals.size(); ++index) {
    if (intervals[index].first <= connected.back().second + kConnectionGapMm)
      connected.back().second =
          std::max(connected.back().second, intervals[index].second);
    else
      connected.push_back(intervals[index]);
  }
  const auto component =
      std::find_if(connected.begin(), connected.end(),
                   [seedLength, kConnectionGapMm](const auto &interval) {
                     return interval.first <= kConnectionGapMm &&
                            interval.second >= -kConnectionGapMm &&
                            interval.second >= seedLength - kConnectionGapMm;
                   });
  if (component == connected.end())
    return std::nullopt;
  const float minimum = component->first;
  const float maximum = component->second;
  Point start{};
  Point end{};
  for (int axis = 0; axis < 3; ++axis) {
    start[axis] = seed.start[axis] + seedDirection[axis] * minimum;
    end[axis] = seed.start[axis] + seedDirection[axis] * maximum;
  }
  return Line{start, end, seed.trussUuid};
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
                                  float toleranceMm) {
  if (fixtureUuids.size() < 2)
    return {.line = std::nullopt, .error = ResolveError::TooFewFixtures};
  for (const std::string &uuid : fixtureUuids) {
    if (scene.fixtures.find(uuid) == scene.fixtures.end())
      return {.line = std::nullopt, .error = ResolveError::MissingFixture};
  }
  std::vector<Line> trussSegments;
  for (const auto &[uuid, truss] : scene.trusses) {
    if (const auto line = BuildLine(truss))
      trussSegments.push_back(*line);
  }
  const float toleranceSquared = toleranceMm * toleranceMm;
  for (std::size_t seedIndex = 0; seedIndex < trussSegments.size();
       ++seedIndex) {
    const auto centerLine = BuildConnectedBridge(trussSegments, seedIndex);
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
