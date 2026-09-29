#include "command_transform.h"

#include "command/command_mutation_transaction.h"
#include "matrixutils.h"
#include "scene_grouping.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>

namespace perastage::command::transform {
namespace {

constexpr float kTransformTolerance = 0.0001f;

// Returns the stable axis name used by machine-facing arguments.
const char *AxisName(int axis) {
  static constexpr const char *names[] = {"x", "y", "z"};
  return axis >= 0 && axis < 3 ? names[axis] : "invalid";
}

// Returns whether every component is structurally valid and finite.
bool Validate(const Command &command, Result &result) {
  if (command.components.empty()) {
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.transform.invalid_combination",
         "A transform requires at least one component."});
    return false;
  }
  if (command.pivotMm && command.kind != Kind::Rotation) {
    result.diagnostics.push_back({DiagnosticSeverity::Error,
                                  DiagnosticPhase::Validation,
                                  "scene.transform.invalid_combination",
                                  "A pivot is supported only for rotation."});
    return false;
  }
  for (const Component &component : command.components) {
    if (component.axis < 0 || component.axis > 2 ||
        (component.values.size() != 1 && component.values.size() != 2) ||
        std::any_of(component.values.begin(), component.values.end(),
                    [](double value) { return !std::isfinite(value); })) {
      result.diagnostics.push_back({DiagnosticSeverity::Error,
                                    DiagnosticPhase::Validation,
                                    "scene.transform.invalid_component",
                                    "Transform axes must be X, Y, or Z and "
                                    "contain one or two finite values.",
                                    "components"});
      return false;
    }
  }
  const bool pivotMode =
      command.components.size() == 1 && command.components.front().group;
  if (command.pivotMm && !pivotMode) {
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.transform.invalid_combination",
         "An explicit pivot requires one grouped rotation component.",
         "pivot_mm"});
    return false;
  }
  if (command.pivotMm &&
      std::any_of(command.pivotMm->begin(), command.pivotMm->end(),
                  [](double value) { return !std::isfinite(value); })) {
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.transform.invalid_component",
         "Pivot coordinates must be finite millimeter values.", "pivot_mm"});
    return false;
  }
  return true;
}

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

// Computes the legacy default pivot from effective target world origins.
std::array<float, 3>
BoundsCenter(const MvrScene &scene,
             const std::vector<scene_grouping::SceneTransformTarget> &targets) {
  auto minimum =
      scene_grouping::GetTargetWorldTransform(scene, targets.front()).o;
  auto maximum = minimum;
  for (const auto &target : targets) {
    const auto origin =
        scene_grouping::GetTargetWorldTransform(scene, target).o;
    for (size_t axis = 0; axis < 3; ++axis) {
      minimum[axis] = std::min(minimum[axis], origin[axis]);
      maximum[axis] = std::max(maximum[axis], origin[axis]);
    }
  }
  return {(minimum[0] + maximum[0]) * 0.5f, (minimum[1] + maximum[1]) * 0.5f,
          (minimum[2] + maximum[2]) * 0.5f};
}

// Compares all effective target matrices using the established tolerance.
bool TargetsChanged(
    const MvrScene &before, const MvrScene &after,
    const std::vector<scene_grouping::SceneTransformTarget> &targets) {
  for (const auto &target : targets) {
    const Matrix left = scene_grouping::GetTargetWorldTransform(before, target);
    const Matrix right = scene_grouping::GetTargetWorldTransform(after, target);
    for (const auto &pair :
         {std::pair{&left.u, &right.u}, std::pair{&left.v, &right.v},
          std::pair{&left.w, &right.w}, std::pair{&left.o, &right.o}})
      for (size_t index = 0; index < 3; ++index)
        if (std::fabs((*pair.first)[index] - (*pair.second)[index]) >
            kTransformTolerance)
          return true;
  }
  return false;
}

} // namespace

// Projects a typed transform command onto the stable generic request contract.
Request BuildRequest(const Command &command) {
  Request request;
  request.commandId =
      command.kind == Kind::Position ? kPositionCommandId : kRotationCommandId;
  for (const Component &component : command.components) {
    const std::string prefix = AxisName(component.axis);
    request.arguments.push_back(
        {prefix +
             (command.kind == Kind::Position ? "_millimeters" : "_degrees"),
         component.values});
    request.arguments.push_back({prefix + "_relative", component.relative});
    request.arguments.push_back(
        {prefix + "_space",
         std::string(component.space == transform_space::TransformSpace::Local
                         ? "local"
                         : "world")});
    request.arguments.push_back({prefix + "_group", component.group});
  }
  if (command.pivotMm)
    request.arguments.push_back(
        {"pivot_mm", std::vector<double>(command.pivotMm->begin(),
                                         command.pivotMm->end())});
  return request;
}

// Validates and executes one semantic transform as one mutation transaction.
Result Execute(const Command &command, ExecutionContext &context,
               const scene_grouping::InteractiveTransformPolicy &policy) {
  Result result;
  result.request = BuildRequest(command);
  if (!Validate(command, result)) {
    result.outcome = Outcome::ValidationError;
    return result;
  }
  const auto targets = scene_grouping::BuildInteractiveTransformTargets(
      context.scene, context.selection, policy);
  if (targets.empty()) {
    result.outcome = Outcome::ValidationError;
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.transform.no_effective_targets",
         "The current selection has no effective transform targets."});
    return result;
  }

  MutationTransaction transaction(context);
  try {
    MvrScene preview = context.scene;
    if (command.kind == Kind::Rotation && command.components.size() == 1 &&
        command.components.front().group) {
      const Component &component = command.components.front();
      const auto pivot =
          command.pivotMm
              ? std::array<float, 3>{static_cast<float>((*command.pivotMm)[0]),
                                     static_cast<float>((*command.pivotMm)[1]),
                                     static_cast<float>((*command.pivotMm)[2])}
              : BoundsCenter(preview, targets);
      scene_grouping::RotateSelectionAroundPivot(
          preview, context.selection, component.axis,
          static_cast<float>(component.values.front()), pivot, component.space,
          policy);
    } else {
      for (const Component &component : command.components) {
        const auto currentTargets =
            scene_grouping::BuildInteractiveTransformTargets(
                preview, context.selection, policy);
        if (command.kind == Kind::Position)
          ApplyPosition(preview, currentTargets, component);
        else
          ApplyRotation(preview, currentTargets, component);
      }
    }
    if (!TargetsChanged(context.scene, preview, targets)) {
      result.diagnostics.push_back(
          {DiagnosticSeverity::Information, DiagnosticPhase::Execution,
           "scene.transform.noop",
           "Transform is already at the requested value."});
      transaction.Rollback();
      return result;
    }
    context.scene = std::move(preview);
    MutationSummary changes;
    changes.sceneChanged = true;
    result.mutation = transaction.Commit(
        changes, command.kind == Kind::Position ? "cli pos" : "cli rot");
  } catch (const std::exception &error) {
    transaction.Rollback();
    result.outcome = Outcome::ExecutionError;
    result.mutation = {};
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Execution,
         "scene.transform.execution_failed",
         std::string("Transform execution failed: ") + error.what()});
  } catch (...) {
    transaction.Rollback();
    result.outcome = Outcome::ExecutionError;
    result.mutation = {};
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Execution,
         "scene.transform.execution_failed", "Transform execution failed."});
  }
  return result;
}

} // namespace perastage::command::transform
