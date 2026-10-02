#include "local_live_controller.h"

#include "command/command_json_serializer.h"
#include "command/command_text_processor.h"
#include "configmanager.h"
#include "project_mutation_host.h"
#include "local_ipc/local_ipc_contract.h"
#include "mainwindow.h"
#include "query/scene_query.h"
#include "selection_movement_settings.h"
#include "json.hpp"

#include <future>
#include <wx/app.h>

namespace {
using Json = nlohmann::ordered_json;

// Serializes the two deliberately exposed read-only live queries.
std::string ExecuteQuery(const std::string &queryId, const MvrScene &scene,
                         const scene_grouping::ObjectSelection &selection) {
  if (queryId == perastage::query::kSummaryQueryId) {
    const auto summary = perastage::query::GetSummary(scene);
    return Json{{"query_id", queryId},
                {"mvr_version_major", summary.mvrVersionMajor},
                {"mvr_version_minor", summary.mvrVersionMinor},
                {"fixtures", summary.fixtures}, {"trusses", summary.trusses},
                {"supports", summary.supports},
                {"scene_objects", summary.sceneObjects},
                {"groups", summary.groups}, {"layers", summary.layers}}
        .dump();
  }
  if (queryId == perastage::query::kSelectionQueryId) {
    Json objects = Json::array();
    for (const auto &object : perastage::query::GetSelection(selection))
      objects.push_back({{"kind", perastage::scene_identity::KindToken(object.kind)},
                         {"uuid", object.uuid}});
    return Json{{"query_id", queryId}, {"objects", std::move(objects)}}.dump();
  }
  return {};
}

} // namespace

// Retains the application refresh owner used by live mutations.
LocalLiveController::LocalLiveController(MainWindow &window) : window_(window) {}

// Stops the endpoint before destroying its application callback.
LocalLiveController::~LocalLiveController() { Stop(); }

// Starts the local live endpoint on the fixed application port.
bool LocalLiveController::Start(std::string &error) {
  return server_.Start(perastage::local_ipc::kDefaultPort,
                       [this](const std::string &request) {
                         return DispatchOnMainThread(request);
                       }, error);
}

// Stops the endpoint before application services are torn down.
void LocalLiveController::Stop() { server_.Stop(); }

// Marshals a transport request to the GUI thread for active-project access.
std::string LocalLiveController::DispatchOnMainThread(
    const std::string &wireRequest) {
  auto promise = std::make_shared<std::promise<std::string>>();
  std::future<std::string> response = promise->get_future();
  wxTheApp->CallAfter([this, promise, wireRequest]() {
    promise->set_value(HandleOnMainThread(wireRequest));
  });
  return response.get();
}

// Executes one validated request against the active project.
std::string LocalLiveController::HandleOnMainThread(
    const std::string &wireRequest) {
  perastage::local_ipc::Request request;
  std::string error;
  if (!perastage::local_ipc::ParseRequest(wireRequest, request, error))
    return error;

  ConfigManager &config = ConfigManager::Get();
  scene_grouping::ObjectSelection selection{
      config.GetSelectedFixtures(), config.GetSelectedTrusses(),
      config.GetSelectedSupports(), config.GetSelectedSceneObjects()};
  if (request.operation == "query") {
    const std::string result = ExecuteQuery(request.value, config.GetScene(), selection);
    if (result.empty())
      return perastage::local_ipc::ErrorResponse(
          request.requestId, "unsupported_operation",
          "The requested query is not exposed by local live IPC.");
    return perastage::local_ipc::SuccessResponse(request.requestId, result);
  }
  if (request.operation != "command")
    return perastage::local_ipc::ErrorResponse(
        request.requestId, "unsupported_operation",
        "Only command and query operations are supported.");

  GuiProjectMutationHost host(config);
  perastage::command::ExecutionContext context{config.GetScene(), selection, host};
  const auto policy =
      selection_movement_settings::LoadInteractiveTransformPolicy(config);
  const auto execution = perastage::command::text::ProcessCommandLine(
      request.value, context, policy);
  Json records = Json::array();
  for (const auto &record : execution.records)
    records.push_back(Json::parse(
        perastage::command::serialization::SerializeResultToJson(record.result)));
  Json parseDiagnostics = Json::array();
  for (const auto &diagnostic : execution.parseDiagnostics)
    parseDiagnostics.push_back({{"code", diagnostic.code},
                                {"message", diagnostic.message}});

  config.SetSelectedFixtures(context.selection.fixtures);
  config.SetSelectedTrusses(context.selection.trusses);
  config.SetSelectedSupports(context.selection.supports);
  config.SetSelectedSceneObjects(context.selection.sceneObjects);
  if (execution.mutation.sceneChanged || execution.mutation.selectionChanged)
    window_.RefreshAfterToolSceneUpdate();

  return perastage::local_ipc::SuccessResponse(
      request.requestId,
      Json{{"success", execution.Success()}, {"records", std::move(records)},
           {"parse_diagnostics", std::move(parseDiagnostics)}}.dump());
}
