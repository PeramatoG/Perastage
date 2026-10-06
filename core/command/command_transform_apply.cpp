#include "command_transform_apply.h"

#include "matrixutils.h"

namespace perastage::command::transform::detail {
namespace {

// Applies one position component to the already resolved effective targets.
void ApplyPosition(
    MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets,
    const Component &component) {
  const double start = component.values.front();
  const double end = component.values.back();
  for (size_t index = 0; index < targets.size(); ++index) {
    const double value =
        component.values.size() == 2 && targets.size() > 1
            ? start + (end - start) * index / (targets.size() - 1)
            : start;
    Matrix matrix =
        scene_grouping::GetTargetWorldTransform(scene, targets[index]);
    if (component.relative) {
      std::array<float, 3> delta{};
      delta[component.axis] = static_cast<float>(value);
      matrix = transform_space::ApplyIncrementalTranslation(matrix, delta,
                                                            component.space);
    } else {
      matrix.o[component.axis] = static_cast<float>(value);
    }
    scene_grouping::SetTargetWorldTransform(scene, targets[index], matrix);
  }
}

// Applies one rotation component with the legacy Console Euler-axis mapping.
void ApplyRotation(
    MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets,
    const Component &component) {
  const double start = component.values.front();
  const double end = component.values.back();
  const int eulerAxis = component.axis == 0 ? 2 : component.axis == 1 ? 1 : 0;
  for (size_t index = 0; index < targets.size(); ++index) {
    const float angle = static_cast<float>(
        component.values.size() == 2 && targets.size() > 1
            ? start + (end - start) * index / (targets.size() - 1)
            : start);
    const Matrix matrix =
        scene_grouping::GetTargetWorldTransform(scene, targets[index]);
    Matrix rotated;
    if (component.relative) {
      Matrix delta =
          component.axis == 0   ? MatrixUtils::EulerToMatrix(0.0f, 0.0f, angle)
          : component.axis == 1 ? MatrixUtils::EulerToMatrix(0.0f, angle, 0.0f)
                                : MatrixUtils::EulerToMatrix(angle, 0.0f, 0.0f);
      rotated = transform_space::ApplyIncrementalRotation(matrix, delta,
                                                          component.space);
    } else {
      auto euler = MatrixUtils::MatrixToEuler(matrix);
      euler[eulerAxis] = angle;
      rotated = MatrixUtils::ApplyRotationPreservingScale(
          matrix, MatrixUtils::EulerToMatrix(euler[0], euler[1], euler[2]),
          matrix.o);
    }
    scene_grouping::SetTargetWorldTransform(scene, targets[index], rotated);
  }
}

} // namespace

// Shares one transform implementation between selection and explicit targets.
void ApplyComponent(
    MvrScene &scene,
    const std::vector<scene_grouping::SceneTransformTarget> &targets, Kind kind,
    const Component &component) {
  if (kind == Kind::Position)
    ApplyPosition(scene, targets, component);
  else
    ApplyRotation(scene, targets, component);
}

} // namespace perastage::command::transform::detail
