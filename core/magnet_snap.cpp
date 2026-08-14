#include "magnet_snap.h"

#include "matrixutils.h"
#include "scene_grouping.h"
#include "truss_attachment_candidates.h"
#include "truss_attachment_paths.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

namespace magnet_snap {
namespace {

// Returns the shared runtime cache used when a caller does not provide one.
truss_attachment_paths::Resolver &DefaultPathResolver() {
  thread_local truss_attachment_paths::Resolver resolver;
  return resolver;
}

struct Bounds {
  Matrix transform{};
  std::array<float, 3> size{0.0f, 0.0f, 0.0f};
};

struct Face {
  std::array<float, 3> center{};
  std::array<float, 3> normal{};
  int axis = 0;
  float sign = 1.0f;
};

// Returns the vector scaled by a scalar factor.
std::array<float, 3> Scale(const std::array<float, 3> &v, float s) {
  return {v[0] * s, v[1] * s, v[2] * s};
}

// Returns the sum of two vectors.
std::array<float, 3> Add(const std::array<float, 3> &a,
                         const std::array<float, 3> &b) {
  return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

// Returns the dot product for two millimeter vectors.
float Dot(const std::array<float, 3> &a, const std::array<float, 3> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Returns the vector difference between two points.
std::array<float, 3> Subtract(const std::array<float, 3> &a,
                              const std::array<float, 3> &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

// Returns the vector length in millimeters.
float Length(const std::array<float, 3> &v) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// Returns the weighted vector length in millimeters.
float WeightedLength(const std::array<float, 3> &v,
                     const std::array<float, 3> &weights) {
  const float x = v[0] * weights[0];
  const float y = v[1] * weights[1];
  const float z = v[2] * weights[2];
  return std::sqrt(x * x + y * y + z * z);
}

// Returns a normalized vector or a safe fallback.
std::array<float, 3> Normalize(const std::array<float, 3> &v) {
  const float len = Length(v);
  if (len <= 1e-5f)
    return {0.0f, 0.0f, 0.0f};
  return {v[0] / len, v[1] / len, v[2] / len};
}

// Builds truss bounds using the truss insertion point as start/base origin.
Bounds MakeTrussBounds(const Truss &truss) {
  const std::array<float, 3> size{std::max(truss.lengthMm, 1.0f),
                                  std::max(truss.widthMm, 1.0f),
                                  std::max(truss.heightMm, 1.0f)};
  Matrix transform = truss.transform;
  transform.o = Add(transform.o, Scale(Normalize(transform.u), size[0] * 0.5f));
  transform.o = Add(transform.o, Scale(Normalize(transform.w), size[2] * 0.5f));
  return Bounds{transform, size};
}

// Appends the eight world-space corners for an oriented bounds box.
void AppendBoundsCorners(const Bounds &bounds,
                         std::vector<std::array<float, 3>> &corners) {
  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(bounds.transform.u), Normalize(bounds.transform.v),
      Normalize(bounds.transform.w)};
  for (int xSign : {-1, 1}) {
    for (int ySign : {-1, 1}) {
      for (int zSign : {-1, 1}) {
        std::array<float, 3> corner = bounds.transform.o;
        corner = Add(corner, Scale(basis[0], bounds.size[0] * 0.5f * xSign));
        corner = Add(corner, Scale(basis[1], bounds.size[1] * 0.5f * ySign));
        corner = Add(corner, Scale(basis[2], bounds.size[2] * 0.5f * zSign));
        corners.push_back(corner);
      }
    }
  }
}

// Collects truss bounds from a group hierarchy while ignoring attached
// fixtures.
void CollectGroupTrussBounds(const MvrScene &scene,
                             const std::string &groupUuid,
                             std::vector<Bounds> &trussBounds) {
  auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return;
  for (const auto &child : groupIt->second.children) {
    if (child.type == MvrNodeType::Truss) {
      auto trussIt = scene.trusses.find(child.uuid);
      if (trussIt != scene.trusses.end())
        trussBounds.push_back(MakeTrussBounds(trussIt->second));
    } else if (child.type == MvrNodeType::GroupObject) {
      CollectGroupTrussBounds(scene, child.uuid, trussBounds);
    }
  }
}

// Builds an aggregate oriented bounds box for grouped trusses only.
std::optional<Bounds> MakeTrussGroupBounds(const MvrScene &scene,
                                           const std::string &groupUuid) {
  std::vector<Bounds> trussBounds;
  CollectGroupTrussBounds(scene, groupUuid, trussBounds);
  if (trussBounds.empty())
    return std::nullopt;

  Bounds aggregate = trussBounds.front();
  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(aggregate.transform.u), Normalize(aggregate.transform.v),
      Normalize(aggregate.transform.w)};
  std::array<float, 3> minLocal{std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max()};
  std::array<float, 3> maxLocal{std::numeric_limits<float>::lowest(),
                                std::numeric_limits<float>::lowest(),
                                std::numeric_limits<float>::lowest()};
  std::vector<std::array<float, 3>> corners;
  for (const Bounds &bounds : trussBounds)
    AppendBoundsCorners(bounds, corners);
  for (const auto &corner : corners) {
    const auto relative = Subtract(corner, aggregate.transform.o);
    for (int axis = 0; axis < 3; ++axis) {
      const float projected = Dot(relative, basis[axis]);
      minLocal[axis] = std::min(minLocal[axis], projected);
      maxLocal[axis] = std::max(maxLocal[axis], projected);
    }
  }

  for (int axis = 0; axis < 3; ++axis) {
    aggregate.size[axis] = std::max(maxLocal[axis] - minLocal[axis], 1.0f);
    aggregate.transform.o =
        Add(aggregate.transform.o,
            Scale(basis[axis], (minLocal[axis] + maxLocal[axis]) * 0.5f));
  }
  return aggregate;
}

// Returns transformed bounds for objects with known dimensions.
std::optional<Bounds> GetBounds(const MvrScene &scene, ObjectType type,
                                const std::string &uuid) {
  if (type == ObjectType::Truss) {
    auto it = scene.trusses.find(uuid);
    if (it == scene.trusses.end())
      return std::nullopt;
    const Truss &t = it->second;
    return MakeTrussBounds(t);
  }
  if (type == ObjectType::TrussGroup)
    return MakeTrussGroupBounds(scene, uuid);
  if (type == ObjectType::Fixture) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      return std::nullopt;
    return Bounds{it->second.transform, {1.0f, 1.0f, 1.0f}};
  }
  auto it = scene.sceneObjects.find(uuid);
  if (it == scene.sceneObjects.end())
    return std::nullopt;
  return Bounds{it->second.transform, {1000.0f, 1000.0f, 1000.0f}};
}

// Builds oriented box face centers from object transform axes.
std::vector<Face> BuildFaces(const Bounds &bounds, bool trussPrimaryFaces) {
  (void)trussPrimaryFaces;
  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(bounds.transform.u), Normalize(bounds.transform.v),
      Normalize(bounds.transform.w)};
  std::vector<Face> faces;
  for (int axis : {0, 1, 2}) {
    const auto offset = Scale(basis[axis], bounds.size[axis] * 0.5f);
    faces.push_back({Add(bounds.transform.o, offset), basis[axis], axis, 1.0f});
    faces.push_back({Add(bounds.transform.o, Scale(offset, -1.0f)),
                     Scale(basis[axis], -1.0f), axis, -1.0f});
  }
  return faces;
}

// Finds the nearest point on a specific oriented bounds face.
std::array<float, 3> ClosestPointOnFace(const Bounds &bounds, const Face &face,
                                        const std::array<float, 3> &point) {
  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(bounds.transform.u), Normalize(bounds.transform.v),
      Normalize(bounds.transform.w)};
  const auto relative = Subtract(point, bounds.transform.o);
  std::array<float, 3> local{};
  for (int axis = 0; axis < 3; ++axis) {
    const float halfSize = bounds.size[axis] * 0.5f;
    local[axis] = Dot(relative, basis[axis]);
    local[axis] = std::clamp(local[axis], -halfSize, halfSize);
  }
  local[face.axis] = face.sign * bounds.size[face.axis] * 0.5f;

  std::array<float, 3> closest = bounds.transform.o;
  for (int axis = 0; axis < 3; ++axis)
    closest = Add(closest, Scale(basis[axis], local[axis]));
  return closest;
}

// Finds the nearest point on an oriented bounds surface to a world-space point.
std::array<float, 3> ClosestPointOnSurface(const Bounds &bounds,
                                           const std::array<float, 3> &point) {
  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(bounds.transform.u), Normalize(bounds.transform.v),
      Normalize(bounds.transform.w)};
  const auto relative = Subtract(point, bounds.transform.o);
  std::array<float, 3> local{};
  std::array<float, 3> halfSize{};
  bool inside = true;
  for (int axis = 0; axis < 3; ++axis) {
    halfSize[axis] = bounds.size[axis] * 0.5f;
    local[axis] = Dot(relative, basis[axis]);
    if (local[axis] < -halfSize[axis] || local[axis] > halfSize[axis])
      inside = false;
    local[axis] = std::clamp(local[axis], -halfSize[axis], halfSize[axis]);
  }

  if (inside) {
    int nearestAxis = 0;
    float nearestDistance = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
      const float positiveDistance = halfSize[axis] - local[axis];
      const float negativeDistance = local[axis] + halfSize[axis];
      const float axisDistance = std::min(positiveDistance, negativeDistance);
      if (axisDistance < nearestDistance) {
        nearestDistance = axisDistance;
        nearestAxis = axis;
      }
    }
    local[nearestAxis] = local[nearestAxis] >= 0.0f ? halfSize[nearestAxis]
                                                    : -halfSize[nearestAxis];
  }

  std::array<float, 3> closest = bounds.transform.o;
  for (int axis = 0; axis < 3; ++axis)
    closest = Add(closest, Scale(basis[axis], local[axis]));
  return closest;
}

// Returns the source object transform target.
scene_grouping::SceneTransformTarget TargetFor(ObjectType type,
                                               const std::string &uuid) {
  switch (type) {
  case ObjectType::Fixture:
    return {MvrNodeType::Fixture, uuid};
  case ObjectType::Truss:
    return {MvrNodeType::Truss, uuid};
  case ObjectType::TrussGroup:
    return {MvrNodeType::GroupObject, uuid};
  case ObjectType::SceneObject:
    return {MvrNodeType::SceneObject, uuid};
  }
  return {MvrNodeType::SceneObject, uuid};
}

// Returns the selection that contains only the snap source object.
scene_grouping::ObjectSelection SelectionForSource(ObjectType type,
                                                   const std::string &uuid) {
  scene_grouping::ObjectSelection selection;
  switch (type) {
  case ObjectType::Fixture:
    selection.fixtures = {uuid};
    break;
  case ObjectType::Truss:
    selection.trusses = {uuid};
    break;
  case ObjectType::TrussGroup:
    break;
  case ObjectType::SceneObject:
    selection.sceneObjects = {uuid};
    break;
  }
  return selection;
}

// Returns the effective transform target that should receive snap translations.
scene_grouping::SceneTransformTarget
SnapTransformTarget(const MvrScene &scene, const SnapResult &result,
                    const scene_grouping::InteractiveTransformPolicy &policy) {
  const auto targets = scene_grouping::BuildInteractiveTransformTargets(
      scene, SelectionForSource(result.sourceType, result.sourceUuid), policy);
  if (!targets.empty())
    return targets.front();

  return TargetFor(result.sourceType, result.sourceUuid);
}

// Returns whether a truss is already a descendant of a group.
bool GroupContainsTruss(const MvrScene &scene, const std::string &groupUuid,
                        const std::string &trussUuid) {
  auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return false;
  for (const auto &child : groupIt->second.children) {
    if (child.type == MvrNodeType::Truss && child.uuid == trussUuid)
      return true;
    if (child.type == MvrNodeType::GroupObject &&
        GroupContainsTruss(scene, child.uuid, trussUuid))
      return true;
  }
  return false;
}

struct CandidateRank {
  double primary = std::numeric_limits<double>::max();
  double depthDifference = std::numeric_limits<double>::max();
  double worldDistance = std::numeric_limits<double>::max();
  std::string targetUuid;
  std::string sourceCandidateId;
  std::string targetCandidateId;
  std::string sourceMemberUuid;
  std::string targetMemberUuid;
};

// Returns whether two candidate directions face one another.
bool DirectionsOppose(const truss_attachment::Candidate &source,
                      const truss_attachment::Candidate &target) {
  return source.worldDirection && target.worldDirection &&
         Dot(*source.worldDirection, *target.worldDirection) <= -0.996f;
}

// Returns whether two oriented bounds have substantial volumetric overlap.
bool SubstantiallyOverlaps(const Bounds &moving, const Bounds &target) {
  constexpr float kAxisEpsilon = 1e-5f;
  constexpr float kSubstantialPenetrationMm = 50.0f;
  const std::array<std::array<float, 3>, 3> a = {Normalize(moving.transform.u),
                                                 Normalize(moving.transform.v),
                                                 Normalize(moving.transform.w)};
  const std::array<std::array<float, 3>, 3> b = {Normalize(target.transform.u),
                                                 Normalize(target.transform.v),
                                                 Normalize(target.transform.w)};
  const std::array<float, 3> aHalf = {
      moving.size[0] * 0.5f, moving.size[1] * 0.5f, moving.size[2] * 0.5f};
  const std::array<float, 3> bHalf = {
      target.size[0] * 0.5f, target.size[1] * 0.5f, target.size[2] * 0.5f};
  float rotation[3][3]{};
  float absoluteRotation[3][3]{};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      rotation[row][column] = Dot(a[row], b[column]);
      absoluteRotation[row][column] =
          std::fabs(rotation[row][column]) + kAxisEpsilon;
    }
  }
  const auto centerDelta = Subtract(target.transform.o, moving.transform.o);
  const std::array<float, 3> translated = {
      Dot(centerDelta, a[0]), Dot(centerDelta, a[1]), Dot(centerDelta, a[2])};
  float minimumFacePenetration = std::numeric_limits<float>::max();
  for (int row = 0; row < 3; ++row) {
    float targetRadius = 0.0f;
    for (int column = 0; column < 3; ++column)
      targetRadius += bHalf[column] * absoluteRotation[row][column];
    const float penetration =
        aHalf[row] + targetRadius - std::fabs(translated[row]);
    if (penetration <= 0.0f)
      return false;
    minimumFacePenetration = std::min(minimumFacePenetration, penetration);
  }
  for (int column = 0; column < 3; ++column) {
    float movingRadius = 0.0f;
    for (int row = 0; row < 3; ++row)
      movingRadius += aHalf[row] * absoluteRotation[row][column];
    const float projectedCenter =
        std::fabs(translated[0] * rotation[0][column] +
                  translated[1] * rotation[1][column] +
                  translated[2] * rotation[2][column]);
    const float penetration = movingRadius + bHalf[column] - projectedCenter;
    if (penetration <= 0.0f)
      return false;
    minimumFacePenetration = std::min(minimumFacePenetration, penetration);
  }
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      const float movingRadius =
          aHalf[(row + 1) % 3] * absoluteRotation[(row + 2) % 3][column] +
          aHalf[(row + 2) % 3] * absoluteRotation[(row + 1) % 3][column];
      const float targetRadius =
          bHalf[(column + 1) % 3] * absoluteRotation[row][(column + 2) % 3] +
          bHalf[(column + 2) % 3] * absoluteRotation[row][(column + 1) % 3];
      const float separation = std::fabs(
          translated[(row + 2) % 3] * rotation[(row + 1) % 3][column] -
          translated[(row + 1) % 3] * rotation[(row + 2) % 3][column]);
      if (separation >= movingRadius + targetRadius)
        return false;
    }
  }
  return minimumFacePenetration > kSubstantialPenetrationMm;
}

// Returns bounds translated by a candidate alignment delta.
Bounds TranslatedBounds(Bounds bounds, const std::array<float, 3> &delta) {
  bounds.transform.o = Add(bounds.transform.o, delta);
  return bounds;
}

// Returns the actual target member bounds for overlap evaluation.
std::optional<Bounds>
TargetMemberBounds(const MvrScene &scene, ObjectType targetType,
                   const std::string &targetUuid,
                   const truss_attachment::Candidate &targetCandidate) {
  const std::string ownerUuid = targetType == ObjectType::Truss
                                    ? targetUuid
                                    : targetCandidate.ownerTrussUuid;
  const auto targetIt = scene.trusses.find(ownerUuid);
  return targetIt == scene.trusses.end()
             ? std::nullopt
             : std::optional<Bounds>(MakeTrussBounds(targetIt->second));
}

// Chooses an alternate inferred source end only to avoid substantial overlap.
const truss_attachment::Candidate &ChooseSourceEndpoint(
    const MvrScene &scene, const SnapSource &source, ObjectType targetType,
    const std::string &targetUuid, const Bounds &sourceBounds,
    const std::vector<truss_attachment::Candidate> &sourceCandidates,
    const truss_attachment::Candidate &acquiredSource,
    const truss_attachment::Candidate &targetCandidate) {
  if (source.type != ObjectType::Truss || sourceCandidates.size() != 2 ||
      acquiredSource.kind !=
          truss_attachment::CandidateKind::InferredLongitudinalEnd ||
      sourceCandidates[0].kind !=
          truss_attachment::CandidateKind::InferredLongitudinalEnd ||
      sourceCandidates[1].kind !=
          truss_attachment::CandidateKind::InferredLongitudinalEnd)
    return acquiredSource;
  const auto targetBounds =
      TargetMemberBounds(scene, targetType, targetUuid, targetCandidate);
  if (!targetBounds)
    return acquiredSource;
  const auto &alternate = &sourceCandidates[0] == &acquiredSource
                              ? sourceCandidates[1]
                              : sourceCandidates[0];
  const auto acquiredDelta = Subtract(targetCandidate.worldTransform.o,
                                      acquiredSource.worldTransform.o);
  const auto alternateDelta =
      Subtract(targetCandidate.worldTransform.o, alternate.worldTransform.o);
  const bool acquiredOverlap = SubstantiallyOverlaps(
      TranslatedBounds(sourceBounds, acquiredDelta), *targetBounds);
  const bool alternateOverlap = SubstantiallyOverlaps(
      TranslatedBounds(sourceBounds, alternateDelta), *targetBounds);
  if (acquiredOverlap != alternateOverlap)
    return acquiredOverlap ? alternate : acquiredSource;
  if (acquiredOverlap && DirectionsOppose(alternate, targetCandidate) !=
                             DirectionsOppose(acquiredSource, targetCandidate))
    return DirectionsOppose(alternate, targetCandidate) ? alternate
                                                        : acquiredSource;
  return acquiredSource;
}

// Returns whether the left candidate rank wins the deterministic ordering.
bool IsBetterRank(const CandidateRank &left, const CandidateRank &right) {
  return std::tie(left.primary, left.depthDifference, left.worldDistance,
                  left.targetUuid, left.sourceCandidateId,
                  left.targetCandidateId, left.sourceMemberUuid,
                  left.targetMemberUuid) <
         std::tie(right.primary, right.depthDifference, right.worldDistance,
                  right.targetUuid, right.sourceCandidateId,
                  right.targetCandidateId, right.sourceMemberUuid,
                  right.targetMemberUuid);
}

// Tests a source and target attachment point and stores the closest snap.
void ConsiderCandidatePair(
    const SnapSource &source, ObjectType targetType,
    const std::string &targetUuid, SnapKind kind,
    const truss_attachment::Candidate &sourceCandidate,
    const truss_attachment::Candidate &targetCandidate, const MvrScene &scene,
    const Bounds &sourceBounds,
    const std::vector<truss_attachment::Candidate> &sourceCandidates,
    const SnapSettings &settings, CandidateRank &bestRank,
    std::optional<SnapResult> &best) {
  const std::array<float, 3> acquisitionDelta = Subtract(
      targetCandidate.worldTransform.o, sourceCandidate.worldTransform.o);
  const auto &resolvedSource =
      ChooseSourceEndpoint(scene, source, targetType, targetUuid, sourceBounds,
                           sourceCandidates, sourceCandidate, targetCandidate);
  const std::array<float, 3> delta = Subtract(targetCandidate.worldTransform.o,
                                              resolvedSource.worldTransform.o);
  const double worldDistance = Length(acquisitionDelta);
  CandidateRank rank;
  rank.targetUuid = targetUuid;
  rank.sourceCandidateId = sourceCandidate.stableId;
  rank.targetCandidateId = targetCandidate.stableId;
  rank.sourceMemberUuid = sourceCandidate.ownerTrussUuid;
  rank.targetMemberUuid = targetCandidate.ownerTrussUuid;
  rank.worldDistance = worldDistance;
  if (settings.trussProjection) {
    const auto sourceProjected = truss_screen_snap::Project(
        *settings.trussProjection, sourceCandidate.worldTransform.o);
    const auto targetProjected = truss_screen_snap::Project(
        *settings.trussProjection, targetCandidate.worldTransform.o);
    if (!sourceProjected || !targetProjected)
      return;
    const double dx = targetProjected->logicalX - sourceProjected->logicalX;
    const double dy = targetProjected->logicalY - sourceProjected->logicalY;
    rank.primary = std::sqrt(dx * dx + dy * dy);
    rank.depthDifference =
        std::fabs(targetProjected->depth - sourceProjected->depth);
    if (rank.primary > settings.trussScreenApertureLogicalPx)
      return;
  } else {
    rank.primary = WeightedLength(acquisitionDelta, settings.axisWeights);
    rank.depthDifference = 0.0;
    if (rank.primary > settings.thresholdMm)
      return;
  }
  if (!IsBetterRank(rank, bestRank))
    return;
  bestRank = rank;
  SnapResult result;
  result.snapped = true;
  result.kind = kind;
  result.sourceUuid = source.uuid;
  result.targetUuid = targetUuid;
  result.sourceType = source.type;
  result.targetType = targetType;
  result.translationDeltaMm = delta;
  result.needsGrouping = kind == SnapKind::TrussToTruss;
  result.sourceCandidateId = resolvedSource.stableId;
  result.targetCandidateId = targetCandidate.stableId;
  result.sourceMemberTrussUuid = resolvedSource.ownerTrussUuid;
  result.targetMemberTrussUuid = targetCandidate.ownerTrussUuid;
  best = result;
}

// Tests a generic object face pair and stores the closest snap result.
void ConsiderFacePair(const SnapSource &source, ObjectType targetType,
                      const std::string &targetUuid, SnapKind kind,
                      const Face &sourceFace, const Bounds &targetBounds,
                      const Face &targetFace, const SnapSettings &settings,
                      float &bestDistance, std::optional<SnapResult> &best) {
  const auto targetPoint =
      ClosestPointOnFace(targetBounds, targetFace, sourceFace.center);
  const auto delta = Subtract(targetPoint, sourceFace.center);
  const float distance = WeightedLength(delta, settings.axisWeights);
  if (distance > settings.thresholdMm || distance >= bestDistance)
    return;
  bestDistance = distance;
  best = SnapResult{
      true,        kind,       source.uuid, targetUuid,
      source.type, targetType, delta,       kind == SnapKind::TrussToTruss};
}

constexpr float kOccupiedJointPositionToleranceMm = 25.0f;
constexpr float kOccupiedJointOpposingDirectionDot = -0.996f;

// Collects truss UUIDs recursively from a GroupObject hierarchy.
void CollectGroupTrussUuids(const MvrScene &scene, const std::string &groupUuid,
                            std::vector<std::string> &uuids) {
  const auto groupIt = scene.groupObjects.find(groupUuid);
  if (groupIt == scene.groupObjects.end())
    return;
  for (const auto &child : groupIt->second.children) {
    if (child.type == MvrNodeType::Truss)
      uuids.push_back(child.uuid);
    else if (child.type == MvrNodeType::GroupObject)
      CollectGroupTrussUuids(scene, child.uuid, uuids);
  }
}

struct JointMatch {
  float distance = 0.0f;
  std::size_t first = 0;
  std::size_t second = 0;
};

// Builds real unoccupied member candidates for a truss group.
std::vector<truss_attachment::Candidate>
BuildGroupCandidatesImpl(const MvrScene &scene, const Bounds &bounds,
                         const std::string &groupUuid,
                         truss_attachment::CandidateResolver &resolver) {
  std::vector<std::string> memberUuids;
  CollectGroupTrussUuids(scene, groupUuid, memberUuids);
  std::sort(memberUuids.begin(), memberUuids.end());
  memberUuids.erase(std::unique(memberUuids.begin(), memberUuids.end()),
                    memberUuids.end());
  std::vector<truss_attachment::Candidate> candidates;
  for (const std::string &uuid : memberUuids) {
    const auto trussIt = scene.trusses.find(uuid);
    if (trussIt == scene.trusses.end())
      continue;
    auto memberCandidates =
        truss_attachment::BuildCandidates(scene, trussIt->second, resolver)
            .candidates;
    candidates.insert(candidates.end(), memberCandidates.begin(),
                      memberCandidates.end());
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const auto &left, const auto &right) {
              return std::tie(left.ownerTrussUuid, left.stableId) <
                     std::tie(right.ownerTrussUuid, right.stableId);
            });
  std::vector<JointMatch> matches;
  for (std::size_t first = 0; first < candidates.size(); ++first) {
    for (std::size_t second = first + 1; second < candidates.size(); ++second) {
      if (candidates[first].ownerTrussUuid == candidates[second].ownerTrussUuid)
        continue;
      const float distance =
          Length(Subtract(candidates[first].worldTransform.o,
                          candidates[second].worldTransform.o));
      if (distance > kOccupiedJointPositionToleranceMm)
        continue;
      if (candidates[first].worldDirection &&
          candidates[second].worldDirection &&
          Dot(*candidates[first].worldDirection,
              *candidates[second].worldDirection) >
              kOccupiedJointOpposingDirectionDot)
        continue;
      matches.push_back({distance, first, second});
    }
  }
  std::sort(
      matches.begin(), matches.end(),
      [&](const JointMatch &left, const JointMatch &right) {
        return std::tie(left.distance, candidates[left.first].ownerTrussUuid,
                        candidates[left.first].stableId,
                        candidates[left.second].ownerTrussUuid,
                        candidates[left.second].stableId) <
               std::tie(right.distance, candidates[right.first].ownerTrussUuid,
                        candidates[right.first].stableId,
                        candidates[right.second].ownerTrussUuid,
                        candidates[right.second].stableId);
      });
  std::vector<bool> occupied(candidates.size(), false);
  for (const JointMatch &match : matches) {
    if (occupied[match.first] || occupied[match.second])
      continue;
    occupied[match.first] = true;
    occupied[match.second] = true;
  }
  std::vector<truss_attachment::Candidate> exposed;
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (!occupied[index])
      exposed.push_back(candidates[index]);
  }
  if (!candidates.empty())
    return exposed;

  Matrix insertionTransform = bounds.transform;
  insertionTransform.o =
      Add(insertionTransform.o,
          Scale(Normalize(insertionTransform.u), -bounds.size[0] * 0.5f));
  insertionTransform.o =
      Add(insertionTransform.o,
          Scale(Normalize(insertionTransform.w), -bounds.size[2] * 0.5f));
  return truss_attachment::BuildAmbiguousCandidates(
      bounds.size, insertionTransform, groupUuid);
}

} // namespace

// Builds deterministic exterior or conservative aggregate group candidates.
std::vector<truss_attachment::Candidate>
BuildTrussGroupCandidates(const MvrScene &scene, const std::string &groupUuid) {
  const auto bounds = MakeTrussGroupBounds(scene, groupUuid);
  truss_attachment::CandidateResolver resolver;
  return bounds ? BuildGroupCandidatesImpl(scene, *bounds, groupUuid, resolver)
                : std::vector<truss_attachment::Candidate>{};
}

// Builds compatible connector or fixture-path references for an active source.
std::vector<AnchorReference>
BuildAnchorReferences(const MvrScene &scene, const SnapSource &source,
                      truss_attachment::CandidateResolver &resolver,
                      truss_attachment_paths::Resolver *pathResolver) {
  std::vector<AnchorReference> references;
  auto appendCandidates = [&](const auto &candidates) {
    for (const auto &candidate : candidates)
      references.push_back(
          {candidate.worldTransform.o, candidate.worldDirection});
  };

  if (source.type == ObjectType::Fixture) {
    return references;
  }

  if (source.type == ObjectType::Truss ||
      source.type == ObjectType::TrussGroup) {
    for (const auto &[uuid, truss] : scene.trusses) {
      (void)uuid;
      appendCandidates(
          truss_attachment::BuildCandidates(scene, truss, resolver).candidates);
    }
    return references;
  }

  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    for (const Face &face : BuildFaces(MakeTrussBounds(truss), false))
      references.push_back({face.center, face.normal});
  }
  for (const auto &[uuid, object] : scene.sceneObjects) {
    (void)uuid;
    for (const Face &face :
         BuildFaces({object.transform, {1000.0f, 1000.0f, 1000.0f}}, false))
      references.push_back({face.center, face.normal});
  }
  return references;
}

// Resolves the same continuous fixture paths used by fixture snapping.
std::vector<AttachmentPathReference>
BuildFixtureAttachmentPathReferences(
    const MvrScene &scene, truss_attachment_paths::Resolver *pathResolver) {
  auto &resolver = pathResolver ? *pathResolver : DefaultPathResolver();
  std::vector<AttachmentPathReference> references;
  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    for (const auto &path : resolver.Resolve(scene, truss).paths) {
      if (path.worldPointsMm.size() >= 2)
        references.push_back({path.stableId, path.worldPointsMm});
    }
  }
  return references;
}

// Finds the best non-destructive Magnet snap candidate for the source object.
std::optional<SnapResult> FindSnap(const MvrScene &scene,
                                   const SnapSource &source,
                                   const SnapSettings &settings) {
  const auto sourceBounds = GetBounds(scene, source.type, source.uuid);
  if (!sourceBounds)
    return std::nullopt;

  float bestDistance = std::numeric_limits<float>::max();
  CandidateRank bestCandidateRank;
  std::optional<SnapResult> best;

  if (source.type == ObjectType::Truss ||
      source.type == ObjectType::TrussGroup) {
    truss_attachment::CandidateResolver localResolver;
    auto &resolver = settings.candidateResolver ? *settings.candidateResolver
                                                : localResolver;
    const auto sourceCandidates =
        source.type == ObjectType::Truss
            ? truss_attachment::BuildCandidates(
                  scene, scene.trusses.at(source.uuid), resolver)
                  .candidates
            : BuildGroupCandidatesImpl(scene, *sourceBounds, source.uuid,
                                       resolver);
    auto considerTrussTarget = [&](ObjectType targetType,
                                   const std::string &uuid,
                                   const Bounds &targetBounds) {
      if (uuid == source.uuid)
        return;
      const auto targetCandidates =
          targetType == ObjectType::Truss
              ? truss_attachment::BuildCandidates(scene, scene.trusses.at(uuid),
                                                  resolver)
                    .candidates
              : BuildGroupCandidatesImpl(scene, targetBounds, uuid, resolver);
      for (const auto &sourceCandidate : sourceCandidates) {
        for (const auto &targetCandidate : targetCandidates)
          ConsiderCandidatePair(
              source, targetType, uuid, SnapKind::TrussToTruss, sourceCandidate,
              targetCandidate, scene, *sourceBounds, sourceCandidates, settings,
              bestCandidateRank, best);
      }
    };
    for (const auto &[uuid, group] : scene.groupObjects) {
      (void)group;
      if (source.type == ObjectType::TrussGroup && uuid == source.uuid)
        continue;
      if (source.type == ObjectType::Truss &&
          GroupContainsTruss(scene, uuid, source.uuid))
        continue;
      if (auto targetBounds = MakeTrussGroupBounds(scene, uuid))
        considerTrussTarget(ObjectType::TrussGroup, uuid, *targetBounds);
    }
    for (const auto &[uuid, truss] : scene.trusses) {
      if (uuid == source.uuid || !truss.parentGroupUuid.empty() ||
          (source.type == ObjectType::TrussGroup &&
           GroupContainsTruss(scene, source.uuid, uuid)))
        continue;
      considerTrussTarget(ObjectType::Truss, uuid, MakeTrussBounds(truss));
    }
    return best;
  }

  if (source.type == ObjectType::Fixture) {
    const std::array<float, 3> insertion = sourceBounds->transform.o;
    auto &resolver = settings.pathResolver ? *settings.pathResolver
                                           : DefaultPathResolver();
    for (const auto &[uuid, truss] : scene.trusses) {
      const Bounds targetBounds = MakeTrussBounds(truss);
      const auto resolution = resolver.Resolve(scene, truss);
      auto consider = [&](const std::array<float, 3> &closest,
                          const std::string &pathId, float pathParameter,
                          truss_attachment_paths::Provenance provenance,
                          float confidence) {
        const std::array<float, 3> delta = Subtract(closest, insertion);
        const float distance = WeightedLength(delta, settings.axisWeights);
        if (distance > settings.thresholdMm || distance >= bestDistance)
          return;
        bestDistance = distance;
        SnapResult result;
        result.snapped = true;
        result.kind = SnapKind::FixtureToTruss;
        result.sourceUuid = source.uuid;
        result.targetUuid = uuid;
        result.sourceType = source.type;
        result.targetType = ObjectType::Truss;
        result.translationDeltaMm = delta;
        result.needsGrouping = true;
        result.targetAttachmentPathId = pathId;
        result.targetAttachmentPathParameter = pathParameter;
        result.targetAttachmentProvenance = provenance;
        result.targetAttachmentConfidence = confidence;
        best = result;
      };
      for (const auto &path : resolution.paths) {
        const auto point = truss_attachment_paths::ClosestPointOnPath(
            path, insertion, true);
        if (point)
          consider(point->pointMm, path.stableId, point->pathParameter,
                   path.provenance, path.diagnostics.confidence);
      }
      if (resolution.paths.empty())
        consider(ClosestPointOnSurface(targetBounds, insertion), {}, 0.0f,
                 truss_attachment_paths::Provenance::ApproximateBoundsFallback,
                 0.0f);
    }
    return best;
  }

  const auto sourceFaces = BuildFaces(*sourceBounds, false);
  auto considerSceneTarget = [&](ObjectType targetType, const std::string &uuid,
                                 const Bounds &bounds) {
    if (targetType == source.type && uuid == source.uuid)
      return;
    for (const auto &sourceFace : sourceFaces) {
      for (const auto &targetFace : BuildFaces(bounds, false))
        ConsiderFacePair(source, targetType, uuid,
                         SnapKind::SceneObjectToObject, sourceFace, bounds,
                         targetFace, settings, bestDistance, best);
    }
  };
  for (const auto &[uuid, truss] : scene.trusses)
    considerSceneTarget(ObjectType::Truss, uuid, MakeTrussBounds(truss));
  for (const auto &[uuid, object] : scene.sceneObjects)
    considerSceneTarget(ObjectType::SceneObject, uuid,
                        {object.transform, {1000.0f, 1000.0f, 1000.0f}});
  return best;
}

// Applies a translation-only snap result through scene_grouping transform
// helpers.
bool ApplySnapTransform(
    MvrScene &scene, const SnapResult &result,
    const scene_grouping::InteractiveTransformPolicy &policy) {
  if (!result.snapped)
    return false;
  auto target = SnapTransformTarget(scene, result, policy);
  Matrix transform = scene_grouping::GetTargetWorldTransform(scene, target);
  for (int axis = 0; axis < 3; ++axis)
    transform.o[axis] += result.translationDeltaMm[axis];
  scene_grouping::SetTargetWorldTransform(scene, target, transform);
  return true;
}

// Creates or extends official MVR GroupObjects after a committed snap.
bool ApplyCommittedSnapGrouping(MvrScene &scene, const SnapResult &result) {
  if (!result.needsGrouping)
    return false;
  if (result.kind == SnapKind::TrussToTruss) {
    if (result.targetType == ObjectType::TrussGroup &&
        result.sourceType == ObjectType::Truss) {
      scene_grouping::ObjectSelection addSelection;
      addSelection.trusses = {result.sourceUuid};
      return scene_grouping::AddSelectionToGroup(scene, addSelection,
                                                 result.targetUuid)
          .changed;
    }
    auto targetIt = scene.trusses.find(result.targetUuid);
    if (targetIt == scene.trusses.end())
      return false;
    if (result.sourceType == ObjectType::TrussGroup) {
      scene_grouping::ObjectSelection addSelection;
      addSelection.trusses = {result.targetUuid};
      return scene_grouping::AddSelectionToGroup(scene, addSelection,
                                                 result.sourceUuid)
          .changed;
    }
    auto sourceIt = scene.trusses.find(result.sourceUuid);
    if (sourceIt == scene.trusses.end())
      return false;
    scene_grouping::ObjectSelection selection;
    selection.trusses = {result.targetUuid, result.sourceUuid};
    if (!targetIt->second.parentGroupUuid.empty()) {
      scene_grouping::ObjectSelection addSelection;
      addSelection.trusses = {result.sourceUuid};
      return scene_grouping::AddSelectionToGroup(
                 scene, addSelection, targetIt->second.parentGroupUuid)
          .changed;
    }
    return scene_grouping::GroupSelection(scene, selection).changed;
  }
  if (result.kind == SnapKind::FixtureToTruss) {
    auto fixtureIt = scene.fixtures.find(result.sourceUuid);
    auto targetIt = scene.trusses.find(result.targetUuid);
    if (fixtureIt == scene.fixtures.end() || targetIt == scene.trusses.end())
      return false;
    if (!targetIt->second.parentGroupUuid.empty()) {
      scene_grouping::ObjectSelection addSelection;
      addSelection.fixtures = {result.sourceUuid};
      return scene_grouping::AddSelectionToGroup(
                 scene, addSelection, targetIt->second.parentGroupUuid)
          .changed;
    }
    scene_grouping::ObjectSelection selection;
    selection.fixtures = {result.sourceUuid};
    selection.trusses = {result.targetUuid};
    return scene_grouping::GroupSelection(scene, selection).changed;
  }
  return false;
}

// Removes the snapped source from its direct GroupObject when it is detached.
bool DetachSnapSourceFromGroup(MvrScene &scene, const SnapResult &result) {
  scene_grouping::ObjectSelection selection;
  if (result.sourceType == ObjectType::Truss)
    selection.trusses = {result.sourceUuid};
  else if (result.sourceType == ObjectType::Fixture)
    selection.fixtures = {result.sourceUuid};
  else
    return false;
  return scene_grouping::RemoveSelectionFromGroup(scene, selection).changed;
}

} // namespace magnet_snap
