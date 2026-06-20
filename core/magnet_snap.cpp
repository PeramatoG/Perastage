#include "magnet_snap.h"

#include "matrixutils.h"
#include "scene_grouping.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace magnet_snap {
namespace {

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

// Returns a snap translation that leaves fully ignored axes untouched.
std::array<float, 3> BuildSnapTranslationDelta(
    const std::array<float, 3> &delta, const SnapSettings &settings) {
  std::array<float, 3> result = delta;
  for (int axis = 0; axis < 3; ++axis) {
    if (settings.axisWeights[axis] <= 1e-5f)
      result[axis] = 0.0f;
  }
  return result;
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

// Collects truss bounds from a group hierarchy while ignoring attached fixtures.
void CollectGroupTrussBounds(const MvrScene &scene, const std::string &groupUuid,
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
    aggregate.transform.o = Add(
        aggregate.transform.o,
        Scale(basis[axis], (minLocal[axis] + maxLocal[axis]) * 0.5f));
  }
  return aggregate;
}

// Returns whether a point lies inside an oriented bounds volume.
bool BoundsContainsPoint(const Bounds &bounds, const std::array<float, 3> &point) {
  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(bounds.transform.u), Normalize(bounds.transform.v),
      Normalize(bounds.transform.w)};
  const auto relative = Subtract(point, bounds.transform.o);
  for (int axis = 0; axis < 3; ++axis) {
    const float projected = Dot(relative, basis[axis]);
    if (std::fabs(projected) > bounds.size[axis] * 0.5f)
      return false;
  }
  return true;
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
scene_grouping::SceneTransformTarget SnapTransformTarget(
    const MvrScene &scene, const SnapResult &result) {
  if (result.sourceType == ObjectType::Fixture)
    return TargetFor(result.sourceType, result.sourceUuid);

  const auto targets = scene_grouping::BuildTransformTargets(
      scene, SelectionForSource(result.sourceType, result.sourceUuid));
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

// Tests a source and target face pair and stores the closest snap result.
void ConsiderFacePair(const SnapSource &source, ObjectType targetType,
                      const std::string &targetUuid, SnapKind kind,
                      const Face &sourceFace, const Bounds &targetBounds,
                      const Face &targetFace, const SnapSettings &settings,
                      float &bestDistance, std::optional<SnapResult> &best) {
  const auto targetPoint =
      ClosestPointOnFace(targetBounds, targetFace, sourceFace.center);
  const std::array<float, 3> delta = Subtract(targetPoint, sourceFace.center);
  const float distance = WeightedLength(delta, settings.axisWeights);
  if (distance > settings.thresholdMm || distance >= bestDistance)
    return;
  bestDistance = distance;
  SnapResult result;
  result.snapped = true;
  result.kind = kind;
  result.sourceUuid = source.uuid;
  result.targetUuid = targetUuid;
  result.sourceType = source.type;
  result.targetType = targetType;
  result.translationDeltaMm = BuildSnapTranslationDelta(delta, settings);
  result.needsGrouping = kind == SnapKind::TrussToTruss;
  best = result;
}

} // namespace

// Finds the best non-destructive Magnet snap candidate for the source object.
std::optional<SnapResult> FindSnap(const MvrScene &scene,
                                   const SnapSource &source,
                                   const SnapSettings &settings) {
  const auto sourceBounds = GetBounds(scene, source.type, source.uuid);
  if (!sourceBounds)
    return std::nullopt;

  float bestDistance = std::numeric_limits<float>::max();
  std::optional<SnapResult> best;

  if (source.type == ObjectType::Truss ||
      source.type == ObjectType::TrussGroup) {
    const auto sourceFaces = BuildFaces(*sourceBounds, true);
    auto considerTrussTarget = [&](ObjectType targetType, const std::string &uuid,
                                   const Bounds &targetBounds) {
      if (uuid == source.uuid)
        return;
      if (targetType == ObjectType::TrussGroup &&
          BoundsContainsPoint(targetBounds, sourceBounds->transform.o))
        return;
      for (const auto &sourceFace : sourceFaces) {
        for (const auto &targetFace : BuildFaces(targetBounds, true))
          ConsiderFacePair(source, targetType, uuid, SnapKind::TrussToTruss,
                           sourceFace, targetBounds, targetFace, settings,
                           bestDistance, best);
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
    for (const auto &[uuid, truss] : scene.trusses) {
      const Bounds targetBounds = MakeTrussBounds(truss);
      const std::array<float, 3> closest =
          ClosestPointOnSurface(targetBounds, insertion);
      const std::array<float, 3> delta = Subtract(closest, insertion);
      const float distance = WeightedLength(delta, settings.axisWeights);
      if (distance > settings.thresholdMm || distance >= bestDistance)
        continue;
      bestDistance = distance;
      SnapResult result;
      result.snapped = true;
      result.kind = SnapKind::FixtureToTruss;
      result.sourceUuid = source.uuid;
      result.targetUuid = uuid;
      result.sourceType = source.type;
      result.targetType = ObjectType::Truss;
      result.translationDeltaMm = BuildSnapTranslationDelta(delta, settings);
      result.needsGrouping = true;
      best = result;
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
        ConsiderFacePair(source, targetType, uuid, SnapKind::SceneObjectToObject,
                         sourceFace, bounds, targetFace, settings, bestDistance,
                         best);
    }
  };
  for (const auto &[uuid, truss] : scene.trusses)
    considerSceneTarget(ObjectType::Truss, uuid, MakeTrussBounds(truss));
  for (const auto &[uuid, object] : scene.sceneObjects)
    considerSceneTarget(ObjectType::SceneObject, uuid,
                        {object.transform, {1000.0f, 1000.0f, 1000.0f}});
  return best;
}

// Applies a translation-only snap result through scene_grouping transform helpers.
bool ApplySnapTransform(MvrScene &scene, const SnapResult &result) {
  if (!result.snapped)
    return false;
  auto target = SnapTransformTarget(scene, result);
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
