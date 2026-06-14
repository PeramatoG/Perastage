#include "magnet_snap.h"

#include "matrixutils.h"
#include "scene_grouping.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace magnet_snap {
namespace {

struct Bounds {
  Matrix transform{};
  std::array<float, 3> size{0.0f, 0.0f, 0.0f};
};

struct Face {
  std::array<float, 3> center{};
  std::array<float, 3> normal{};
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

// Returns the vector length in millimeters.
float Length(const std::array<float, 3> &v) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// Returns a normalized vector or a safe fallback.
std::array<float, 3> Normalize(const std::array<float, 3> &v) {
  const float len = Length(v);
  if (len <= 1e-5f)
    return {0.0f, 0.0f, 0.0f};
  return {v[0] / len, v[1] / len, v[2] / len};
}

// Returns transformed bounds for objects with known dimensions.
std::optional<Bounds> GetBounds(const MvrScene &scene, ObjectType type,
                                const std::string &uuid) {
  if (type == ObjectType::Truss) {
    auto it = scene.trusses.find(uuid);
    if (it == scene.trusses.end())
      return std::nullopt;
    const Truss &t = it->second;
    return Bounds{t.transform, {std::max(t.lengthMm, 1.0f),
                                std::max(t.widthMm, 1.0f),
                                std::max(t.heightMm, 1.0f)}};
  }
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
  std::vector<int> axes{0, 1, 2};
  if (trussPrimaryFaces) {
    const auto maxIt = std::max_element(bounds.size.begin(), bounds.size.end());
    const int longAxis = static_cast<int>(std::distance(bounds.size.begin(), maxIt));
    const float minSize = *std::min_element(bounds.size.begin(), bounds.size.end());
    const bool elongated = bounds.size[longAxis] >= minSize * 1.4f;
    if (elongated)
      axes = {longAxis};
  }

  const std::array<std::array<float, 3>, 3> basis = {
      Normalize(bounds.transform.u), Normalize(bounds.transform.v),
      Normalize(bounds.transform.w)};
  std::vector<Face> faces;
  for (int axis : axes) {
    const auto offset = Scale(basis[axis], bounds.size[axis] * 0.5f);
    faces.push_back({Add(bounds.transform.o, offset), basis[axis]});
    faces.push_back({Add(bounds.transform.o, Scale(offset, -1.0f)),
                     Scale(basis[axis], -1.0f)});
  }
  return faces;
}

// Returns the source object transform target.
scene_grouping::SceneTransformTarget TargetFor(ObjectType type,
                                               const std::string &uuid) {
  switch (type) {
  case ObjectType::Fixture:
    return {MvrNodeType::Fixture, uuid};
  case ObjectType::Truss:
    return {MvrNodeType::Truss, uuid};
  case ObjectType::SceneObject:
    return {MvrNodeType::SceneObject, uuid};
  }
  return {MvrNodeType::SceneObject, uuid};
}

// Tests a source and target face pair and stores the closest snap result.
void ConsiderFacePair(const SnapSource &source, ObjectType targetType,
                      const std::string &targetUuid, SnapKind kind,
                      const Face &sourceFace, const Face &targetFace,
                      float thresholdMm, float &bestDistance,
                      std::optional<SnapResult> &best) {
  const std::array<float, 3> delta = {targetFace.center[0] - sourceFace.center[0],
                                     targetFace.center[1] - sourceFace.center[1],
                                     targetFace.center[2] - sourceFace.center[2]};
  const float distance = Length(delta);
  if (distance > thresholdMm || distance >= bestDistance)
    return;
  bestDistance = distance;
  SnapResult result;
  result.snapped = true;
  result.kind = kind;
  result.sourceUuid = source.uuid;
  result.targetUuid = targetUuid;
  result.sourceType = source.type;
  result.targetType = targetType;
  result.translationDeltaMm = delta;
  result.needsTrussGrouping = kind == SnapKind::TrussToTruss;
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

  if (source.type == ObjectType::Truss) {
    const auto sourceFaces = BuildFaces(*sourceBounds, true);
    for (const auto &[uuid, truss] : scene.trusses) {
      if (uuid == source.uuid)
        continue;
      const Bounds targetBounds{truss.transform,
                                {std::max(truss.lengthMm, 1.0f),
                                 std::max(truss.widthMm, 1.0f),
                                 std::max(truss.heightMm, 1.0f)}};
      for (const auto &sourceFace : sourceFaces) {
        for (const auto &targetFace : BuildFaces(targetBounds, true))
          ConsiderFacePair(source, ObjectType::Truss, uuid,
                           SnapKind::TrussToTruss, sourceFace, targetFace,
                           settings.thresholdMm, bestDistance, best);
      }
    }
    return best;
  }

  if (source.type == ObjectType::Fixture) {
    const Face insertion{sourceBounds->transform.o, {0.0f, 0.0f, 1.0f}};
    for (const auto &[uuid, truss] : scene.trusses) {
      const Bounds targetBounds{truss.transform,
                                {std::max(truss.lengthMm, 1.0f),
                                 std::max(truss.widthMm, 1.0f),
                                 std::max(truss.heightMm, 1.0f)}};
      for (const auto &targetFace : BuildFaces(targetBounds, false))
        ConsiderFacePair(source, ObjectType::Truss, uuid,
                         SnapKind::FixtureToTruss, insertion, targetFace,
                         settings.thresholdMm, bestDistance, best);
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
                         sourceFace, targetFace, settings.thresholdMm,
                         bestDistance, best);
    }
  };
  for (const auto &[uuid, truss] : scene.trusses)
    considerSceneTarget(ObjectType::Truss, uuid,
                        {truss.transform, {std::max(truss.lengthMm, 1.0f),
                                           std::max(truss.widthMm, 1.0f),
                                           std::max(truss.heightMm, 1.0f)}});
  for (const auto &[uuid, object] : scene.sceneObjects)
    considerSceneTarget(ObjectType::SceneObject, uuid,
                        {object.transform, {1000.0f, 1000.0f, 1000.0f}});
  return best;
}

// Applies a translation-only snap result through scene_grouping transform helpers.
bool ApplySnapTransform(MvrScene &scene, const SnapResult &result) {
  if (!result.snapped)
    return false;
  auto target = TargetFor(result.sourceType, result.sourceUuid);
  Matrix transform = scene_grouping::GetTargetWorldTransform(scene, target);
  for (int axis = 0; axis < 3; ++axis)
    transform.o[axis] += result.translationDeltaMm[axis];
  scene_grouping::SetTargetWorldTransform(scene, target, transform);
  return true;
}

// Creates or extends an official MVR GroupObject after a committed truss snap.
bool ApplyCommittedTrussGrouping(MvrScene &scene, const SnapResult &result) {
  if (!result.needsTrussGrouping || result.sourceType != ObjectType::Truss ||
      result.targetType != ObjectType::Truss)
    return false;
  auto sourceIt = scene.trusses.find(result.sourceUuid);
  auto targetIt = scene.trusses.find(result.targetUuid);
  if (sourceIt == scene.trusses.end() || targetIt == scene.trusses.end())
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

} // namespace magnet_snap
