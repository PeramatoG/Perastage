#include "capability/capability_catalog.h"

#include "command/command_operation_ids.h"
#include "query/query_contract.h"

#include <algorithm>
#include <set>
#include <type_traits>

namespace perastage::capability {
namespace {
using A = ArgumentDescriptor;

// Returns the authoritative, deliberately sorted capability inventory.
const std::vector<Descriptor> &Inventory() {
  static std::vector<Descriptor> catalog = {
      {command::scene_tools::kFixtureToSupportCommandId,
       OperationKind::Command,
       "Replace fixtures with supports.",
       Effect::Destructive,
       {{"fixture_uuids", ArgumentType::StringList, true,
         "Fixture UUIDs to convert."}},
       {{"desktop_gui", ExposureState::Full}}},
      {command::scene_tools::kSceneObjectsToTrussesCommandId,
       OperationKind::Command,
       "Replace same-model scene objects with trusses.",
       Effect::Destructive,
       {{"source_scene_object_uuid", ArgumentType::String, true,
         "Source SceneObject UUID."}},
       {{"desktop_gui", ExposureState::Full}}},
      {command::scene_tools::kGroupCreateCommandId,
       OperationKind::Command,
       "Create a group from explicitly identified scene objects.",
       Effect::Mutating,
       {{"fixture_uuids", ArgumentType::StringList, true, "Fixture UUIDs."},
        {"truss_uuids", ArgumentType::StringList, true, "Truss UUIDs."},
        {"support_uuids", ArgumentType::StringList, true, "Support UUIDs."},
        {"scene_object_uuids", ArgumentType::StringList, true,
         "SceneObject UUIDs."}},
       {{"desktop_gui", ExposureState::Full}}},
      {command::scene_tools::kGroupUngroupCommandId,
       OperationKind::Command,
       "Remove groups containing explicitly identified scene objects.",
       Effect::Mutating,
       {{"fixture_uuids", ArgumentType::StringList, true, "Fixture UUIDs."},
        {"truss_uuids", ArgumentType::StringList, true, "Truss UUIDs."},
        {"support_uuids", ArgumentType::StringList, true, "Support UUIDs."},
        {"scene_object_uuids", ArgumentType::StringList, true,
         "SceneObject UUIDs."}},
       {{"desktop_gui", ExposureState::Full}}},
      {query::kGroupsQueryId,
       OperationKind::Query,
       "List scene groups.",
       Effect::ReadOnly,
       {},
       {}},
      {query::kLayersQueryId,
       OperationKind::Query,
       "List scene layers.",
       Effect::ReadOnly,
       {},
       {}},
      {query::kObjectQueryId,
       OperationKind::Query,
       "Get one typed scene object.",
       Effect::ReadOnly,
       {{"object_kind", ArgumentType::String, true,
         "Stable scene object kind."},
        {"object_uuid", ArgumentType::String, true, "Object UUID."}},
       {}},
      {query::kObjectsQueryId,
       OperationKind::Query,
       "List scene objects.",
       Effect::ReadOnly,
       {},
       {}},
      {query::kPatchQueryId,
       OperationKind::Query,
       "Report fixture patch status and conflicts.",
       Effect::ReadOnly,
       {},
       {}},
      {command::selection::kClearCommandId,
       OperationKind::Command,
       "Clear the Console-compatible selection.",
       Effect::Mutating,
       {},
       {{"development_cli", ExposureState::Full},
        {"embedded_console", ExposureState::Full},
        {"local_live_cli", ExposureState::Full},
        {"mcp", ExposureState::Full},
        {"osc", ExposureState::Full}}},
      {query::kSelectionQueryId,
       OperationKind::Query,
       "Get the current typed selection.",
       Effect::ReadOnly,
       {},
       {{"local_live_cli", ExposureState::Full},
        {"mcp", ExposureState::Full}}},
      {command::selection::kUpdateCommandId,
       OperationKind::Command,
       "Update a typed object selection.",
       Effect::Mutating,
       {{"target_kind", ArgumentType::String, true, "Selection object kind."},
        {"preserve_existing", ArgumentType::Boolean, true,
         "Whether existing targets are retained."},
        {"operation_kinds", ArgumentType::StringList, true,
         "Ordered add or remove operations."},
        {"object_kinds", ArgumentType::StringList, true,
         "Ordered object kinds."},
        {"object_uuids", ArgumentType::StringList, true,
         "Ordered object UUIDs."}},
       {{"development_cli", ExposureState::Partial},
        {"embedded_console", ExposureState::Partial},
        {"local_live_cli", ExposureState::Partial}}},
      {query::kSummaryQueryId,
       OperationKind::Query,
       "Summarize scene contents.",
       Effect::ReadOnly,
       {},
       {{"local_live_cli", ExposureState::Full},
        {"mcp", ExposureState::Full}}},
      {command::transform::kPositionCommandId,
       OperationKind::Command,
       "Set or offset selected-object positions.",
       Effect::Mutating,
       {},
       {{"development_cli", ExposureState::Full},
        {"embedded_console", ExposureState::Full},
        {"local_live_cli", ExposureState::Full},
        {"mcp", ExposureState::Partial},
        {"osc", ExposureState::Partial}}},
      {command::transform::kRotationCommandId,
       OperationKind::Command,
       "Set or offset selected-object rotations.",
       Effect::Mutating,
       {},
       {{"development_cli", ExposureState::Full},
        {"embedded_console", ExposureState::Full},
        {"local_live_cli", ExposureState::Full},
        {"mcp", ExposureState::Partial},
        {"osc", ExposureState::Partial}}},
  };
  static const bool initialized = [] {
    for (Descriptor &descriptor : catalog) {
      if (descriptor.operationId != command::transform::kPositionCommandId &&
          descriptor.operationId != command::transform::kRotationCommandId)
        continue;
      const bool position =
          descriptor.operationId == command::transform::kPositionCommandId;
      for (std::string_view axis : {"x", "y", "z"}) {
        static const char *positionValues[] = {"x_millimeters", "y_millimeters",
                                               "z_millimeters"};
        static const char *rotationValues[] = {"x_degrees", "y_degrees",
                                               "z_degrees"};
        static const char *relative[] = {"x_relative", "y_relative",
                                         "z_relative"};
        static const char *space[] = {"x_space", "y_space", "z_space"};
        static const char *group[] = {"x_group", "y_group", "z_group"};
        const auto index = axis == "x" ? 0 : axis == "y" ? 1 : 2;
        descriptor.arguments.push_back(
            {position ? positionValues[index] : rotationValues[index],
             ArgumentType::Float64List, false,
             "One value or an inclusive distribution range."});
        descriptor.arguments.push_back({relative[index], ArgumentType::Boolean,
                                        false,
                                        "Whether the component is relative."});
        descriptor.arguments.push_back({space[index], ArgumentType::String,
                                        false,
                                        "World or local transform space."});
        descriptor.arguments.push_back(
            {group[index], ArgumentType::Boolean, false,
             "Whether targets transform as a group."});
      }
      if (!position)
        descriptor.arguments.push_back(
            {"pivot_mm", ArgumentType::Float64List, false,
             "Three explicit pivot coordinates in millimeters."});
    }
    return true;
  }();
  (void)initialized;
  return catalog;
}

} // namespace

// Maps one Command argument value to the stable capability type vocabulary.
ArgumentType TypeOfArgumentValue(const command::ArgumentValue &value) {
  return std::visit(
      [](const auto &typedValue) {
        using T = std::decay_t<decltype(typedValue)>;
        if constexpr (std::is_same_v<T, bool>)
          return ArgumentType::Boolean;
        else if constexpr (std::is_same_v<T, std::int64_t>)
          return ArgumentType::Int64;
        else if constexpr (std::is_same_v<T, double>)
          return ArgumentType::Float64;
        else if constexpr (std::is_same_v<T, std::string>)
          return ArgumentType::String;
        else if constexpr (std::is_same_v<T, std::vector<std::int64_t>>)
          return ArgumentType::Int64List;
        else if constexpr (std::is_same_v<T, std::vector<double>>)
          return ArgumentType::Float64List;
        else {
          static_assert(std::is_same_v<T, std::vector<std::string>>);
          return ArgumentType::StringList;
        }
      },
      value);
}

// Returns the immutable catalog in ascending operation-ID order.
std::span<const Descriptor> Catalog() { return Inventory(); }

// Finds a descriptor by its stable operation identifier.
const Descriptor *Find(std::string_view id) {
  const auto &catalog = Inventory();
  const auto found =
      std::lower_bound(catalog.begin(), catalog.end(), id,
                       [](const Descriptor &item, std::string_view value) {
                         return item.operationId < value;
                       });
  return found != catalog.end() && found->operationId == id ? &*found : nullptr;
}

// Checks only the generic shape of a Command request against its descriptor.
std::vector<RequestShapeIssue>
ValidateRequestShape(const command::Request &request) {
  std::vector<RequestShapeIssue> issues;
  const Descriptor *descriptor = Find(request.commandId);
  if (!descriptor || descriptor->kind != OperationKind::Command)
    return {{RequestShapeIssueKind::UnknownOperation, {}}};
  std::set<std::string_view> seen;
  for (const auto &argument : request.arguments) {
    if (!seen.insert(argument.id).second) {
      issues.push_back({RequestShapeIssueKind::DuplicateArgument, argument.id});
      continue;
    }
    const auto found = std::find_if(
        descriptor->arguments.begin(), descriptor->arguments.end(),
        [&](const A &candidate) { return candidate.id == argument.id; });
    if (found == descriptor->arguments.end())
      issues.push_back({RequestShapeIssueKind::UnknownArgument, argument.id});
    else if (found->type != TypeOfArgumentValue(argument.value))
      issues.push_back({RequestShapeIssueKind::WrongType, argument.id});
  }
  for (const A &argument : descriptor->arguments)
    if (argument.required && !seen.contains(argument.id))
      issues.push_back(
          {RequestShapeIssueKind::MissingArgument, std::string(argument.id)});
  return issues;
}

// Returns the stable token for an operation kind.
const char *Token(OperationKind value) {
  return value == OperationKind::Command ? "command" : "query";
}
// Returns the stable token for an argument type.
const char *Token(ArgumentType value) {
  static constexpr const char *tokens[] = {
      "boolean",    "int64",        "float64",    "string",
      "int64_list", "float64_list", "string_list"};
  return tokens[static_cast<int>(value)];
}
// Returns the stable token for an effect classification.
const char *Token(Effect value) {
  static constexpr const char *tokens[] = {"read_only", "mutating",
                                           "destructive"};
  return tokens[static_cast<int>(value)];
}
// Returns the stable token for a frontend exposure state.
const char *Token(ExposureState value) {
  return value == ExposureState::Full ? "full" : "partial";
}
} // namespace perastage::capability
