#include "fixture_line_distribution.h"

#include <algorithm>
#include <cmath>

namespace fixture_line_distribution {
namespace {

using Point = std::array<float, 3>;

// Returns squared Euclidean distance between two points.
float DistanceSquared(const Point &lhs, const Point &rhs) {
  float result = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    const float delta = lhs[axis] - rhs[axis];
    result += delta * delta;
  }
  return result;
}

// Returns a straight segment spanning one resolved attachment path.
std::optional<Line> PathSegment(const truss_attachment_paths::Path &path) {
  if (path.worldPointsMm.size() < 2)
    return std::nullopt;
  return Line{path.worldPointsMm.front(), path.worldPointsMm.back(),
              path.ownerTrussUuid};
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

// Merges connected collinear attachment paths into one hang line.
std::optional<Line>
BuildConnectedPath(const std::vector<truss_attachment_paths::Path> &paths,
                   std::size_t seedIndex) {
  constexpr float kParallelCosine = 0.999f;
  constexpr float kChordToleranceMm = 75.0f;
  constexpr float kConnectionGapMm = 150.0f;
  const auto seedLine = PathSegment(paths[seedIndex]);
  if (!seedLine)
    return std::nullopt;
  Point seedDirection{};
  float seedLength = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    seedDirection[axis] = seedLine->end[axis] - seedLine->start[axis];
    seedLength += seedDirection[axis] * seedDirection[axis];
  }
  seedLength = std::sqrt(seedLength);
  if (seedLength <= 1e-3f)
    return std::nullopt;
  for (float &value : seedDirection)
    value /= seedLength;

  std::vector<std::pair<float, float>> intervals;
  for (const auto &path : paths) {
    const auto segment = PathSegment(path);
    if (!segment)
      continue;
    Point direction{};
    float length = 0.0f;
    for (int axis = 0; axis < 3; ++axis) {
      direction[axis] = segment->end[axis] - segment->start[axis];
      length += direction[axis] * direction[axis];
    }
    length = std::sqrt(length);
    if (length <= 1e-3f)
      continue;
    float cosine = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
      cosine += seedDirection[axis] * direction[axis] / length;
    if (std::fabs(cosine) < kParallelCosine ||
        DistanceSquared(segment->start,
                        ProjectOntoInfiniteLine(*seedLine, segment->start)) >
            kChordToleranceMm * kChordToleranceMm)
      continue;
    float start = ParameterAlongLine(*seedLine, segment->start);
    float end = ParameterAlongLine(*seedLine, segment->end);
    if (start > end)
      std::swap(start, end);
    intervals.emplace_back(start, end);
  }
  if (intervals.empty())
    return std::nullopt;
  std::sort(intervals.begin(), intervals.end());
  std::vector<std::pair<float, float>> components{intervals.front()};
  for (std::size_t index = 1; index < intervals.size(); ++index) {
    if (intervals[index].first <= components.back().second + kConnectionGapMm)
      components.back().second =
          std::max(components.back().second, intervals[index].second);
    else
      components.push_back(intervals[index]);
  }
  const auto component =
      std::find_if(components.begin(), components.end(),
                   [seedLength, kConnectionGapMm](const auto &interval) {
                     return interval.first <= kConnectionGapMm &&
                            interval.second >= seedLength - kConnectionGapMm;
                   });
  if (component == components.end())
    return std::nullopt;
  Point start{};
  Point end{};
  for (int axis = 0; axis < 3; ++axis) {
    start[axis] =
        seedLine->start[axis] + seedDirection[axis] * component->first;
    end[axis] = seedLine->start[axis] + seedDirection[axis] * component->second;
  }
  return Line{start, end, paths[seedIndex].ownerTrussUuid};
}

// Converts a merged line into a path for shared closest-point calculations.
truss_attachment_paths::Path AsAttachmentPath(const Line &line) {
  truss_attachment_paths::Path path;
  path.ownerTrussUuid = line.trussUuid;
  path.worldPointsMm = {line.start, line.end};
  return path;
}

} // namespace

// Projects a world point onto a finite truss line.
Point ProjectOntoLine(const Line &line, const Point &point) {
  const auto closest = truss_attachment_paths::ClosestPointOnPath(
      AsAttachmentPath(line), point, true);
  return closest ? closest->pointMm : line.start;
}

// Resolves the shared connected attachment path containing every fixture.
ResolveResult ResolveSelectedLine(
    const MvrScene &scene, const std::vector<std::string> &fixtureUuids,
    const std::vector<truss_attachment_paths::Path> &attachmentPaths,
    float toleranceMm) {
  if (fixtureUuids.size() < 2)
    return {.line = std::nullopt, .error = ResolveError::TooFewFixtures};
  for (const std::string &uuid : fixtureUuids) {
    if (scene.fixtures.find(uuid) == scene.fixtures.end())
      return {.line = std::nullopt, .error = ResolveError::MissingFixture};
  }
  for (std::size_t seedIndex = 0; seedIndex < attachmentPaths.size();
       ++seedIndex) {
    const auto seedClosest = truss_attachment_paths::ClosestPointOnPath(
        attachmentPaths[seedIndex],
        scene.fixtures.at(fixtureUuids.front()).transform.o, true);
    if (!seedClosest || seedClosest->distanceMm > toleranceMm)
      continue;
    const auto line = BuildConnectedPath(attachmentPaths, seedIndex);
    if (!line)
      continue;
    const auto mergedPath = AsAttachmentPath(*line);
    const bool containsAll = std::all_of(
        fixtureUuids.begin(), fixtureUuids.end(), [&](const std::string &uuid) {
          const auto closest = truss_attachment_paths::ClosestPointOnPath(
              mergedPath, scene.fixtures.at(uuid).transform.o, true);
          return closest && closest->distanceMm <= toleranceMm;
        });
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

// Distributes fixtures at an exact center or edge gap within a directed
// segment.
SpacingResult ApplySpacing(MvrScene &scene,
                           const std::vector<std::string> &fixtureUuids,
                           const Point &start, const Point &directionPoint,
                           const SpacingOptions &options) {
  SpacingResult result;
  Point direction{};
  for (int axis = 0; axis < 3; ++axis) {
    direction[axis] = directionPoint[axis] - start[axis];
    result.availableLengthMm += direction[axis] * direction[axis];
  }
  result.availableLengthMm = std::sqrt(result.availableLengthMm);
  if (fixtureUuids.size() < 2 || result.availableLengthMm <= 1e-3f ||
      options.spacingMm < 0.0f)
    return result;
  for (float &value : direction)
    value /= result.availableLengthMm;

  std::vector<float> gaps(fixtureUuids.size() - 1, options.spacingMm);
  if (options.reference == SpacingReference::FixtureEdges) {
    if (options.halfExtentsMm.size() != fixtureUuids.size())
      return result;
    for (std::size_t index = 0; index < gaps.size(); ++index)
      gaps[index] +=
          options.halfExtentsMm[index] + options.halfExtentsMm[index + 1];
  }
  for (float gap : gaps)
    result.requiredLengthMm += gap;
  result.fits = result.requiredLengthMm <= result.availableLengthMm + 0.01f;
  if (!result.fits)
    return result;

  std::vector<float> offsets(fixtureUuids.size(), 0.0f);
  if (options.origin == SpacingOrigin::FromPointInDirection) {
    for (std::size_t index = 1; index < offsets.size(); ++index)
      offsets[index] = offsets[index - 1] + gaps[index - 1];
  } else {
    std::vector<float> ordered(fixtureUuids.size(), 0.0f);
    const float margin =
        (result.availableLengthMm - result.requiredLengthMm) * 0.5f;
    ordered[0] = margin;
    for (std::size_t index = 1; index < ordered.size(); ++index)
      ordered[index] = ordered[index - 1] + gaps[index - 1];
    std::size_t left = 0;
    std::size_t right = ordered.size() - 1;
    for (std::size_t index = 0; index < offsets.size(); ++index) {
      offsets[index] = index % 2 == 0 ? ordered[left++] : ordered[right--];
    }
  }
  for (std::size_t index = 0; index < fixtureUuids.size(); ++index) {
    auto fixture = scene.fixtures.find(fixtureUuids[index]);
    if (fixture == scene.fixtures.end())
      return result;
    for (int axis = 0; axis < 3; ++axis)
      fixture->second.transform.o[axis] =
          start[axis] + direction[axis] * offsets[index];
  }
  result.applied = true;
  return result;
}

} // namespace fixture_line_distribution
