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

  std::ostringstream rejectedOut;
  std::ostringstream rejectedErr;
  const std::array<std::string_view, 4> rejectedArgs = {
      "query", "scene.objects.list", "--port", port};
  assert(perastage::cli::RunLive(rejectedArgs, rejectedOut, rejectedErr) == 4);

  std::ostringstream failedCommandOut;
  std::ostringstream failedCommandErr;
  const std::array<std::string_view, 4> failedCommandArgs = {
      "command", "unknown", "--port", port};
  assert(perastage::cli::RunLive(failedCommandArgs, failedCommandOut,
                                 failedCommandErr) == 4);

  server.Stop();

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
