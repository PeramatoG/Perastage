#include "live/live_transform_batch_adapter.h"

#include "command/command_json_serializer.h"
#include "command/command_transform_batch.h"
#include "json.hpp"

#include <array>
#include <string_view>

namespace perastage::live {
namespace {
using Json = nlohmann::ordered_json;
constexpr std::array<const char *, 7> kArgumentIds = {
    "target_kinds", "target_uuids", "component_kinds", "axes",
    "values", "modes", "spaces"};

bool HasTypedArrays(const Json &arguments) {
  if (arguments.size() != kArgumentIds.size())
    return false;
  for (const char *id : kArgumentIds) {
    if (!arguments.contains(id) || !arguments[id].is_array())
      return false;
    for (const auto &value : arguments[id])
      if (std::string_view(id) == "values" ? !value.is_number()
                                           : !value.is_string())
        return false;
  }
  return true;
}
} // namespace

ExecutionResult ExecuteTransformBatch(const local_ipc::Request &request,
                                     command::ExecutionContext &context) {
  if (request.value != command::transform::kBatchCommandId)
    return {local_ipc::ErrorResponse(
                request.requestId, "unsupported_operation",
                "The requested semantic command is not exposed by local live IPC."),
            {}};
  const Json arguments = request.argumentsJson.empty()
                             ? Json::object()
                             : Json::parse(request.argumentsJson);
  if (!HasTypedArrays(arguments))
    return {local_ipc::ErrorResponse(
                request.requestId, "invalid_arguments",
                "Batch transforms require target_kinds, target_uuids, "
                "component_kinds, axes, modes, spaces string arrays and a "
                "numeric values array."),
            {}};

  command::Request batch{command::transform::kBatchCommandId, {}};
  for (const char *id : kArgumentIds) {
    if (std::string_view(id) == "values")
      batch.arguments.push_back({id, arguments[id].get<std::vector<double>>()});
    else
      batch.arguments.push_back(
          {id, arguments[id].get<std::vector<std::string>>()});
  }
  const auto execution = command::transform::ExecuteBatch(batch, context);
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
