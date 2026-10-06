#include "json.hpp"
#include "live/live_request_executor.h"
#include "live_command.h"
#include "local_ipc/local_ipc_transport.h"
#include "matrixutils.h"
#include "mvrscene.h"

#include <array>
#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace {

class RecordingHost final : public perastage::command::ProjectMutationHost {
public:
  // Records publication through the real semantic mutation transaction.
  perastage::command::MutationPublication
  CommitMutation(const MvrScene &, const scene_grouping::ObjectSelection &,
                 const std::string &) override {
    ++publications;
    return {true, true};
  }

  int publications = 0;
};

// Builds a real live request using the public schema.
std::string Request(const std::string &id, const std::string &operation,
                    const std::string &value) {
  return nlohmann::ordered_json{{"schema_version", 1},
                                {"request_id", id},
                                {"operation", operation},
                                {"value", value}}
      .dump();
}

} // namespace

// Verifies real semantic execution and CLI round trips share one live scene.
int main() {
  std::string rejectedRequestResponse;
  std::string rejectedRequestError;
  assert(!perastage::local_ipc::Exchange(
      0, std::string(perastage::local_ipc::kMaximumMessageBytes + 1, 'x'),
      rejectedRequestResponse, rejectedRequestError));
  assert(rejectedRequestResponse.empty() &&
         rejectedRequestError == "request_too_large");

  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture-a";
  fixture.fixtureId = 1;
  fixture.transform = MatrixUtils::Identity();
  scene.fixtures.emplace(fixture.uuid, fixture);
  scene_grouping::ObjectSelection selection;
  RecordingHost host;
  perastage::command::ExecutionContext context{scene, selection, host};
  scene_grouping::InteractiveTransformPolicy policy;

  perastage::local_ipc::Server server;
  std::string transportError;
  assert(server.Start(
      0,
      [&](const std::string &wire) {
        return perastage::live::ExecuteRequest(wire, context, policy).response;
      },
      transportError));

  const std::string port = std::to_string(server.Port());
  std::ostringstream commandOut;
  std::ostringstream commandErr;
  const std::array<std::string_view, 4> commandArgs = {"command", "f 1 pos x 2",
                                                       "--port", port};
  assert(perastage::cli::RunLive(commandArgs, commandOut, commandErr) == 0);
  assert(commandErr.str().empty());
  assert(host.publications == 1);
  assert(std::fabs(scene.fixtures.at(fixture.uuid).transform.o[0] - 2000.0f) <
         0.01f);
  assert(selection.fixtures == std::vector<std::string>{fixture.uuid});
  assert(selection.trusses.empty());
  assert(selection.supports.empty());
  assert(selection.sceneObjects.empty());

  std::ostringstream queryOut;
  std::ostringstream queryErr;
  const std::array<std::string_view, 4> queryArgs = {
      "query", "scene.selection.get", "--port", port};
  assert(perastage::cli::RunLive(queryArgs, queryOut, queryErr) == 0);
  const auto query = nlohmann::json::parse(queryOut.str());
  assert(query["result"]["objects"].size() == 1);
  assert(query["result"]["objects"][0]["uuid"] == fixture.uuid);

  std::ostringstream clearOut;
  std::ostringstream clearErr;
  const std::array<std::string_view, 4> clearArgs = {"command", "clear",
                                                     "--port", port};
  assert(perastage::cli::RunLive(clearArgs, clearOut, clearErr) == 0);
  assert(clearErr.str().empty());
  assert(selection.fixtures.empty());
  assert(selection.trusses.empty());
  assert(selection.supports.empty());
  assert(selection.sceneObjects.empty());

  std::ostringstream objectsOut;
  std::ostringstream objectsErr;
  const std::array<std::string_view, 4> objectsArgs = {
      "query", "scene.objects.list", "--port", port};
  assert(perastage::cli::RunLive(objectsArgs, objectsOut, objectsErr) == 0);
  assert(objectsErr.str().empty());
  const auto objects = nlohmann::json::parse(objectsOut.str());
  assert(objects["result"]["objects"].size() == 1);
  assert(objects["result"]["objects"][0]["uuid"] == fixture.uuid);

  const int publicationsBeforeBatch = host.publications;
  const auto batchSelection = selection;
  nlohmann::json batchArguments = {
      {"target_kinds", {"fixture", "fixture"}},
      {"target_uuids", {fixture.uuid, fixture.uuid}},
      {"component_kinds", {"position", "position"}}, {"axes", {"x", "y"}},
      {"values", {2500, 500}}, {"modes", {"absolute", "absolute"}},
      {"spaces", {"world", "world"}}};
  auto runBatch = [&](int expectedExit) {
    const std::string arguments = batchArguments.dump();
    const std::array<std::string_view, 6> args = {
        "execute", "scene.transform.batch", "--args", arguments, "--port", port};
    std::ostringstream out;
    std::ostringstream err;
    assert(perastage::cli::RunLive(args, out, err) == expectedExit);
    assert(err.str().empty());
    return nlohmann::json::parse(out.str());
  };
  assert(runBatch(0)["result"]["success"] == true);
  assert(host.publications == publicationsBeforeBatch + 1);
  assert(scene.fixtures.at(fixture.uuid).transform.o[0] == 2500.0f);
  assert(scene.fixtures.at(fixture.uuid).transform.o[1] == 500.0f);
  assert(runBatch(0)["result"]["records"][0]["diagnostics"][0]["code"] ==
         "scene.transform.batch.noop");
  batchArguments["target_uuids"][1] = "missing";
  batchArguments["values"][0] = 3000;
  const auto failedBatch = runBatch(4);
  assert(failedBatch["ok"] == true && failedBatch["result"]["success"] == false);
  assert(failedBatch["result"]["records"][0]["outcome"] == "validation_error");
  assert(host.publications == publicationsBeforeBatch + 1);
  assert(scene.fixtures.at(fixture.uuid).transform.o[0] == 2500.0f);
  assert(selection.fixtures == batchSelection.fixtures &&
         selection.trusses == batchSelection.trusses &&
         selection.supports == batchSelection.supports &&
         selection.sceneObjects == batchSelection.sceneObjects);

  std::ostringstream failedCommandOut;
  std::ostringstream failedCommandErr;
  const std::array<std::string_view, 4> failedCommandArgs = {
      "command", "unknown", "--port", port};
  assert(perastage::cli::RunLive(failedCommandArgs, failedCommandOut,
                                 failedCommandErr) == 4);

  // Real discovery responses exceed the old request-sized response cap.
  for (int index = 0; index < 250; ++index) {
    Fixture discovered = fixture;
    discovered.uuid = "discovered-" + std::to_string(index);
    discovered.instanceName = "Front fixture " + std::to_string(index);
    discovered.typeName = "Deterministic discovery type";
    discovered.gdtfSpec = "Vendor@Discovery.gdtf";
    discovered.address = "1.101";
    scene.fixtures.emplace(discovered.uuid, discovered);
  }
  const int publicationsBeforeDiscovery = host.publications;
  const auto selectionBeforeDiscovery = selection;
  std::ostringstream largeOut;
  std::ostringstream largeErr;
  assert(perastage::cli::RunLive(objectsArgs, largeOut, largeErr) == 0);
  assert(largeErr.str().empty());
  assert(largeOut.str().size() > perastage::local_ipc::kMaximumMessageBytes);
  const auto largeResponse = nlohmann::json::parse(largeOut.str());
  assert(largeResponse["result"]["objects"].size() == 251);
  assert(host.publications == publicationsBeforeDiscovery);
  assert(selection.fixtures == selectionBeforeDiscovery.fixtures &&
         selection.trusses == selectionBeforeDiscovery.trusses &&
         selection.supports == selectionBeforeDiscovery.supports &&
         selection.sceneObjects == selectionBeforeDiscovery.sceneObjects);

  server.Stop();

  perastage::local_ipc::Server oversizedServer;
  assert(oversizedServer.Start(
      0,
      [](const std::string &) {
        return std::string(perastage::local_ipc::kMaximumResponseBytes + 1, 'x');
      },
      transportError));
  std::string oversizedResponse;
  assert(perastage::local_ipc::Exchange(
      oversizedServer.Port(), Request("oversized", "query", "scene.summary"),
      oversizedResponse, transportError));
  const auto oversized = nlohmann::json::parse(oversizedResponse);
  assert(oversized["ok"] == false && oversized["request_id"] == "oversized");
  assert(oversized["error"]["code"] == "response_too_large");
  assert(host.publications == publicationsBeforeDiscovery &&
         scene.fixtures.size() == 251 && selection.fixtures.empty());
  oversizedServer.Stop();

  perastage::local_ipc::Server throwingServer;
  assert(throwingServer.Start(
      0,
      [](const std::string &) -> std::string {
        throw std::runtime_error("handler failure");
      },
      transportError));
  std::string failureResponse;
  assert(perastage::local_ipc::Exchange(
      throwingServer.Port(), Request("failure", "query", "scene.summary"),
      failureResponse, transportError));
  assert(nlohmann::json::parse(failureResponse)["error"]["code"] ==
         "internal_error");
  throwingServer.Stop();
  return 0;
}
