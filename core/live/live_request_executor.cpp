#include "live/live_request_executor.h"

#include "command/command_json_serializer.h"
#include "command/command_text_processor.h"
#include "json.hpp"
#include "live/live_query_adapter.h"
#include "live/live_selection_adapter.h"
#include "local_ipc/local_ipc_contract.h"

namespace perastage::live {
namespace {
using Json = nlohmann::ordered_json;
} // namespace

// Executes one local-live wire request against an explicitly supplied context.
ExecutionResult ExecuteRequest(
    const std::string &wireRequest, command::ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &transformPolicy) {
  local_ipc::Request request;
  std::string error;
  if (!local_ipc::ParseRequest(wireRequest, request, error))
    return {std::move(error), {}};

  if (request.operation == "query")
    return ExecuteQuery(request, context);
  if (request.operation == "execute")
    return ExecuteSelectionUpdate(request, context);
  if (request.operation != "command")
    return {local_ipc::ErrorResponse(
                request.requestId, "unsupported_operation",
                "Only command, query, and execute operations are supported."),
            {}};
  if (!request.argumentsJson.empty())
    return {local_ipc::ErrorResponse(
                request.requestId, "invalid_arguments",
                "Text commands do not accept structured arguments."),
            {}};

  const auto execution = command::text::ProcessCommandLine(
      request.value, context, transformPolicy);
  Json records = Json::array();
  for (const auto &record : execution.records)
    records.push_back(Json::parse(
        command::serialization::SerializeResultToJson(record.result)));
  Json parseDiagnostics = Json::array();
  for (const auto &diagnostic : execution.parseDiagnostics)
    parseDiagnostics.push_back(
        {{"code", diagnostic.code}, {"message", diagnostic.message}});
  return {local_ipc::SuccessResponse(
              request.requestId,
              Json{{"success", execution.Success()},
                   {"records", std::move(records)},
                   {"parse_diagnostics", std::move(parseDiagnostics)}}
                  .dump()),
          execution.mutation};
}

} // namespace perastage::live
