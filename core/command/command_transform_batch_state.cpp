#include "command_transform_batch_state.h"

#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace perastage::command::transform::detail {
namespace {

constexpr float kTransformTolerance = 0.0001f;

struct TransformState {
  Matrix world;
  Matrix local;
  bool localMeaningful = false;
};

// Reads the exact stored state rather than promoting or resolving selection.
TransformState State(const MvrScene &scene,
                     const scene_grouping::SceneTransformTarget &target) {
  switch (target.type) {
  case MvrNodeType::Fixture: {
    const auto &object = scene.fixtures.at(target.uuid);
    return {object.transform, object.localTransform, !object.parentGroupUuid.empty()};
  }
  case MvrNodeType::Truss: {
    const auto &object = scene.trusses.at(target.uuid);
    return {object.transform, object.localTransform, !object.parentGroupUuid.empty()};
  }
  case MvrNodeType::Support: {
    const auto &object = scene.supports.at(target.uuid);
    return {object.transform, object.localTransform, !object.parentGroupUuid.empty()};
  }
  case MvrNodeType::SceneObject: {
    const auto &object = scene.sceneObjects.at(target.uuid);
    return {object.transform, object.localTransform, !object.parentGroupUuid.empty()};
  }
  case MvrNodeType::GroupObject: {
    const auto &object = scene.groupObjects.at(target.uuid);
    return {object.transform, object.localTransform,
            !object.parentGroupUuid.empty()};
  }
  }
  throw std::runtime_error("Unsupported batch transform target.");
}

// Reports whether every scene matrix component remains representable.
bool Finite(const Matrix &matrix) {
  for (const auto *vector : {&matrix.u, &matrix.v, &matrix.w, &matrix.o})
    for (float value : *vector)
      if (!std::isfinite(value))
        return false;
  return true;
}

// Applies the established semantic transform tolerance to every matrix field.
bool Different(const Matrix &left, const Matrix &right) {
  for (const auto &pair :
       {std::pair{&left.u, &right.u}, std::pair{&left.v, &right.v},
        std::pair{&left.w, &right.w}, std::pair{&left.o, &right.o}})
    for (size_t axis = 0; axis < 3; ++axis)
      if (std::fabs((*pair.first)[axis] - (*pair.second)[axis]) >
          kTransformTolerance)
        return true;
  return false;
}

// Traverses the same typed group children synchronized by the shared setter.
void Collect(const MvrScene &scene,
             const scene_grouping::SceneTransformTarget &target,
             std::unordered_set<std::string> &seen,
             std::vector<scene_grouping::SceneTransformTarget> &affected) {
  const std::string identity = std::to_string(static_cast<int>(target.type)) +
                               ":" + target.uuid;
  if (!seen.insert(identity).second)
    return;
  // Imported group references can refer to absent nodes; the setter skips them.
  switch (target.type) {
  case MvrNodeType::Fixture:
    if (!scene.fixtures.contains(target.uuid)) return;
    break;
  case MvrNodeType::Truss:
    if (!scene.trusses.contains(target.uuid)) return;
    break;
  case MvrNodeType::Support:
    if (!scene.supports.contains(target.uuid)) return;
    break;
  case MvrNodeType::SceneObject:
    if (!scene.sceneObjects.contains(target.uuid)) return;
    break;
  case MvrNodeType::GroupObject:
    if (!scene.groupObjects.contains(target.uuid)) return;
    break;
  }
  affected.push_back(target);
  if (target.type == MvrNodeType::GroupObject)
    for (const auto &child : scene.groupObjects.at(target.uuid).children)
      Collect(scene, {child.type, child.uuid}, seen, affected);
}

} // namespace

// Resolves deterministic affected-state coverage including nested groups.
std::vector<scene_grouping::SceneTransformTarget> CollectAffectedTargets(
    const MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets) {
  std::vector<scene_grouping::SceneTransformTarget> affected;
  std::unordered_set<std::string> seen;
  for (const auto &target : targets)
    Collect(scene, target, seen, affected);
  return affected;
}

// Stops preview execution before non-finite matrices can reach publication.
void RequireFiniteTransforms(
    const MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets) {
  for (const auto &target : targets) {
    const TransformState state = State(scene, target);
    if (!Finite(state.world) || (state.localMeaningful && !Finite(state.local)))
      throw std::overflow_error("Batch preview produced a non-finite transform.");
  }
}

// Detects descendant/local changes even when their parent ends where it began.
bool BatchTransformsChanged(
    const MvrScene &before, const MvrScene &after,
    const std::vector<scene_grouping::SceneTransformTarget> &targets) {
  for (const auto &target : targets) {
    const TransformState left = State(before, target);
    const TransformState right = State(after, target);
    if (Different(left.world, right.world) ||
        (left.localMeaningful &&
         (!right.localMeaningful || Different(left.local, right.local))))
      return true;
  }
  return false;
}

} // namespace perastage::command::transform::detail
