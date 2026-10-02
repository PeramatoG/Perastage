#include "live/live_request_executor.h"

#include "command/command_json_serializer.h"
#include "command/command_text_processor.h"
#include "json.hpp"
#include "local_ipc/local_ipc_contract.h"
#include "query/scene_query.h"

namespace perastage::live {
namespace {
using Json = nlohmann::ordered_json;

// Serializes one deliberately supported live query.
std::string ExecuteQuery(const std::string &queryId, const MvrScene &scene,
                         const scene_grouping::ObjectSelection &selection) {
  if (queryId == query::kSummaryQueryId) {
    const auto summary = query::GetSummary(scene);
    return Json{{"query_id", queryId},
                {"mvr_version_major", summary.mvrVersionMajor},
                {"mvr_version_minor", summary.mvrVersionMinor},
                {"fixtures", summary.fixtures},
                {"trusses", summary.trusses},
                {"supports", summary.supports},
                {"scene_objects", summary.sceneObjects},
                {"groups", summary.groups},
                {"layers", summary.layers}}
        .dump();
  }
  if (queryId == query::kSelectionQueryId) {
    Json objects = Json::array();
    for (const auto &object : query::GetSelection(selection))
      objects.push_back({{"kind", scene_identity::KindToken(object.kind)},
                         {"uuid", object.uuid}});
    return Json{{"query_id", queryId}, {"objects", std::move(objects)}}.dump();
  }
  return {};
}

} // namespace

// Executes one local-live wire request against an explicitly supplied context.
ExecutionResult ExecuteRequest(
    const std::string &wireRequest, command::ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &transformPolicy) {
  local_ipc::Request request;
  std::string error;
  if (!local_ipc::ParseRequest(wireRequest, request, error))
    return {std::move(error), {}};

  if (request.operation == "query") {
    const std::string result =
        ExecuteQuery(request.value, context.scene, context.selection);
    if (result.empty())
      return {local_ipc::ErrorResponse(
                  request.requestId, "unsupported_operation",
                  "The requested query is not exposed by local live IPC."),
              {}};
    return {local_ipc::SuccessResponse(request.requestId, result), {}};
  }
  if (request.operation != "command")
    return {local_ipc::ErrorResponse(
                request.requestId, "unsupported_operation",
                "Only command and query operations are supported."),
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
