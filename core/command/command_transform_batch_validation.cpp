#include "command_transform_batch_validation.h"

#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>

namespace perastage::command::transform::detail {
namespace {

constexpr std::array<std::string_view, 7> kArgumentIds{
    "target_kinds", "target_uuids", "component_kinds", "axes", "values",
    "modes", "spaces"};

// Records one stable validation error at the offending aligned argument.
bool Reject(Result &result, const char *code, std::string message,
            std::string argumentId) {
  result.outcome = Outcome::ValidationError;
  result.diagnostics.push_back({DiagnosticSeverity::Error,
                                DiagnosticPhase::Validation, code,
                                std::move(message), std::move(argumentId)});
  return false;
}

// Resolves stable kind tokens to exact existing transformable node kinds.
std::optional<MvrNodeType> ResolveKind(const std::string &kind) {
  if (kind == "fixture")
    return MvrNodeType::Fixture;
  if (kind == "truss")
    return MvrNodeType::Truss;
  if (kind == "support")
    return MvrNodeType::Support;
  if (kind == "scene_object")
    return MvrNodeType::SceneObject;
  if (kind == "group")
    return MvrNodeType::GroupObject;
  return std::nullopt;
}

// Checks the UUID against its declared map instead of an untyped lookup.
bool Exists(const MvrScene &scene,
            const scene_grouping::SceneTransformTarget &target) {
  switch (target.type) {
  case MvrNodeType::Fixture:
    return scene.fixtures.contains(target.uuid);
  case MvrNodeType::Truss:
    return scene.trusses.contains(target.uuid);
  case MvrNodeType::Support:
    return scene.supports.contains(target.uuid);
  case MvrNodeType::SceneObject:
    return scene.sceneObjects.contains(target.uuid);
  case MvrNodeType::GroupObject:
    return scene.groupObjects.contains(target.uuid);
  }
  return false;
}

} // namespace

// Validates complete aligned typed lists before any speculative scene mutation.
bool ValidateBatch(const Request &request, const MvrScene &scene,
                   std::vector<BatchRow> &rows, Result &result) {
  if (request.commandId != kBatchCommandId)
    return Reject(result, "scene.transform.batch.invalid_arguments",
                  "Expected the scene.transform.batch command ID.", "command_id");
  std::array<const Argument *, kArgumentIds.size()> arguments{};
  for (const auto &argument : request.arguments) {
    size_t index = 0;
    while (index < kArgumentIds.size() && kArgumentIds[index] != argument.id)
      ++index;
    if (index == kArgumentIds.size() || arguments[index])
      return Reject(result, "scene.transform.batch.invalid_arguments",
                    "Batch arguments must be unique supported aligned lists.",
                    argument.id);
    arguments[index] = &argument;
  }
  for (size_t index = 0; index < arguments.size(); ++index)
    if (!arguments[index])
      return Reject(result, "scene.transform.batch.invalid_arguments",
                    "A required batch argument is missing.",
                    std::string(kArgumentIds[index]));

  std::array<const std::vector<std::string> *, kArgumentIds.size()> strings{};
  const auto *values =
      std::get_if<std::vector<double>>(&arguments[4]->value);
  if (!values)
    return Reject(result, "scene.transform.batch.invalid_arguments",
                  "Batch values must be a typed numeric list.", "values");
  for (size_t index = 0; index < arguments.size(); ++index) {
    if (index == 4)
      continue;
    strings[index] =
        std::get_if<std::vector<std::string>>(&arguments[index]->value);
    if (!strings[index])
      return Reject(result, "scene.transform.batch.invalid_arguments",
                    "Batch token and UUID arguments must be typed string lists.",
                    std::string(kArgumentIds[index]));
  }
  if (values->empty())
    return Reject(result, "scene.transform.batch.invalid_arguments",
                  "Batch aligned lists must have non-zero length.", "values");
  for (size_t index = 0; index < strings.size(); ++index)
    if (index != 4 && strings[index]->size() != values->size())
      return Reject(result, "scene.transform.batch.invalid_arguments",
                    "All batch lists must have identical non-zero length.",
                    std::string(kArgumentIds[index]));

  rows.reserve(values->size());
  for (size_t index = 0; index < values->size(); ++index) {
    const auto kind = ResolveKind((*strings[0])[index]);
    const std::string row = "Batch row " + std::to_string(index) + ": ";
    if (!kind)
      return Reject(result, "scene.transform.batch.invalid_target",
                    row + "unsupported target kind.", "target_kinds");
    scene_grouping::SceneTransformTarget target{*kind, (*strings[1])[index]};
    if (target.uuid.empty() || !Exists(scene, target))
      return Reject(result, "scene.transform.batch.invalid_target",
                    row + "UUID must identify an existing object of its declared kind.",
                    "target_uuids");
    const auto &componentKind = (*strings[2])[index];
    const auto &axis = (*strings[3])[index];
    const auto &mode = (*strings[5])[index];
    const auto &space = (*strings[6])[index];
    for (const auto &invalid : {
             std::pair{componentKind != "position" && componentKind != "rotation",
                       "component_kinds"},
             std::pair{axis != "x" && axis != "y" && axis != "z", "axes"},
             std::pair{mode != "absolute" && mode != "relative", "modes"},
             std::pair{space != "world" && space != "local", "spaces"},
             std::pair{!std::isfinite((*values)[index]), "values"}}) {
      if (invalid.first)
        return Reject(result, "scene.transform.batch.invalid_component",
                      row + "component tokens must be supported and values finite.",
                      invalid.second);
    }
    BatchRow resolved;
    resolved.target = std::move(target);
    resolved.kind = componentKind == "position" ? Kind::Position : Kind::Rotation;
    resolved.component.axis = axis == "x" ? 0 : axis == "y" ? 1 : 2;
    resolved.component.values = {(*values)[index]};
    resolved.component.relative = mode == "relative";
    resolved.component.space = space == "local"
                                   ? transform_space::TransformSpace::Local
                                   : transform_space::TransformSpace::World;
    rows.push_back(std::move(resolved));
  }
  return true;
}

} // namespace perastage::command::transform::detail
