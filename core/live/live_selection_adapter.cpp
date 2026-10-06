#include "live/live_selection_adapter.h"

#include "command/command_json_serializer.h"
#include "command/command_selection.h"
#include "json.hpp"

#include <optional>

namespace perastage::live {
namespace {
using Json = nlohmann::ordered_json;
namespace selection = command::selection;

std::optional<selection::ObjectKind> ObjectKind(const std::string &token) {
  if (token == "fixture")
    return selection::ObjectKind::Fixture;
  if (token == "truss")
    return selection::ObjectKind::Truss;
  if (token == "support")
    return selection::ObjectKind::Support;
  if (token == "scene_object")
    return selection::ObjectKind::SceneObject;
  return std::nullopt;
}

bool StringArray(const Json &arguments, const char *key) {
  if (!arguments.contains(key) || !arguments[key].is_array())
    return false;
  for (const auto &value : arguments[key])
    if (!value.is_string())
      return false;
  return true;
}

ExecutionResult InvalidArguments(const local_ipc::Request &request) {
  return {local_ipc::ErrorResponse(
              request.requestId, "invalid_arguments",
              "Selection requires target_kind, preserve_existing, and equally "
              "sized operation_kinds, object_kinds, object_uuids string arrays."),
          {}};
}

} // namespace

ExecutionResult ExecuteSelectionUpdate(const local_ipc::Request &request,
                                       command::ExecutionContext &context) {
  if (request.value != selection::kUpdateCommandId)
    return {local_ipc::ErrorResponse(
                request.requestId, "unsupported_operation",
                "The requested semantic command is not exposed by local live IPC."),
            {}};
  const Json arguments = request.argumentsJson.empty()
                             ? Json::object()
                             : Json::parse(request.argumentsJson);
  if (arguments.size() != 5 || !arguments.contains("target_kind") ||
      !arguments["target_kind"].is_string() ||
      !arguments.contains("preserve_existing") ||
      !arguments["preserve_existing"].is_boolean() ||
      !StringArray(arguments, "operation_kinds") ||
      !StringArray(arguments, "object_kinds") ||
      !StringArray(arguments, "object_uuids"))
    return InvalidArguments(request);
  const auto target = ObjectKind(arguments["target_kind"].get<std::string>());
  const auto count = arguments["object_uuids"].size();
  if (!target || arguments["operation_kinds"].size() != count ||
      arguments["object_kinds"].size() != count)
    return InvalidArguments(request);
  selection::Command update;
  update.target = *target;
  update.preserveExisting = arguments["preserve_existing"].get<bool>();
  for (std::size_t index = 0; index < count; ++index) {
    const auto &mode = arguments["operation_kinds"][index];
    const auto kind = ObjectKind(arguments["object_kinds"][index].get<std::string>());
    if (!kind || (mode != "add" && mode != "remove"))
      return InvalidArguments(request);
    update.operations.push_back(
        {mode == "add" ? selection::OperationKind::Add
                        : selection::OperationKind::Remove,
         {{*kind, arguments["object_uuids"][index].get<std::string>()}}});
  }
  const auto execution = selection::Execute(update, context);
  return {local_ipc::SuccessResponse(
              request.requestId,
              Json{{"success", execution.Success()},
                   {"records", Json::array({Json::parse(
                       command::serialization::SerializeResultToJson(execution))})},
                   {"parse_diagnostics", Json::array()}}
                  .dump()),
          execution.mutation};
}

} // namespace perastage::live
