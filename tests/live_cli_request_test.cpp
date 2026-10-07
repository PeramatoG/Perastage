#include "json.hpp"
#include "live_command.h"
#include "local_ipc/local_ipc_contract.h"
#include "local_ipc/local_ipc_transport.h"

#include <cassert>
#include <mutex>
#include <sstream>
#include <vector>

namespace {
using Json = nlohmann::json;

// Verifies typed argv is forwarded as one versioned, structured request.
void CheckRequest(const std::vector<std::string_view> &args,
                  const Json &expected, std::vector<Json> &requests,
                  std::mutex &mutex, int expectedExit = 0) {
  std::ostringstream out;
  std::ostringstream err;
  assert(perastage::cli::RunLive(args, out, err) == expectedExit);
  assert(err.str().empty());
  const auto response = Json::parse(out.str());
  assert(response["schema_version"] == 1 && response["request_id"] == "cli-1");
  std::lock_guard lock(mutex);
  assert(!requests.empty() && requests.back() == expected);
}
} // namespace

// Exercises generic query/execute encoding and local argument validation.
int main() {
  std::mutex mutex;
  std::vector<Json> requests;
  perastage::local_ipc::Server server;
  std::string transportError;
  assert(server.Start(
      0,
      [&](const std::string &wire) {
        const auto request = Json::parse(wire);
        {
          std::lock_guard lock(mutex);
          requests.push_back(request);
        }
        const bool success = request["value"] != "test.failure";
        return perastage::local_ipc::SuccessResponse(
            request["request_id"].get<std::string>(),
            Json{{"success", success}}.dump());
      },
      transportError));
  const std::string port = std::to_string(server.Port());
  for (const std::string id : {"scene.summary", "scene.selection.get",
                              "scene.objects.list", "scene.layers.list",
                              "scene.groups.list"})
    CheckRequest({"query", id, "--port", port},
                 {{"schema_version", 1}, {"request_id", "cli-1"},
                  {"operation", "query"}, {"value", id}}, requests, mutex);

  const std::string objectArguments =
      R"json({"kind":"fixture","uuid":"fixture-a; clear $(printf ignored)"})json";
  CheckRequest({"query", "scene.object.get", "--args", objectArguments,
                "--port", port},
               {{"schema_version", 1}, {"request_id", "cli-1"},
                {"operation", "query"}, {"value", "scene.object.get"},
                {"arguments", Json::parse(objectArguments)}}, requests, mutex);
  const Json arguments = {{"target_kind", "support"},
                          {"preserve_existing", false},
                          {"operation_kinds", {"add", "remove"}},
                          {"object_kinds", {"support", "support"}},
                          {"object_uuids", {"support-a", "support-b"}}};
  const std::string argumentText = arguments.dump();
  CheckRequest({"execute", "scene.selection.update", "--port", port,
                "--args", argumentText},
               {{"schema_version", 1}, {"request_id", "cli-1"},
                {"operation", "execute"}, {"value", "scene.selection.update"},
                {"arguments", arguments}}, requests, mutex);
  const Json batchArguments = {
      {"target_kinds", {"fixture", "support"}},
      {"target_uuids", {"fixture-a; clear $(printf ignored)", "support-a"}},
      {"component_kinds", {"position", "rotation"}},
      {"axes", {"x", "z"}}, {"values", {1250, 45.5}},
      {"modes", {"absolute", "relative"}}, {"spaces", {"world", "local"}}};
  const std::string batchText = batchArguments.dump();
  CheckRequest({"execute", "scene.transform.batch", "--args", batchText,
                "--port", port},
               {{"schema_version", 1}, {"request_id", "cli-1"},
                {"operation", "execute"}, {"value", "scene.transform.batch"},
                {"arguments", batchArguments}}, requests, mutex);
  CheckRequest({"execute", "test.failure", "--args", "{}", "--port", port},
               {{"schema_version", 1}, {"request_id", "cli-1"},
                {"operation", "execute"}, {"value", "test.failure"},
                {"arguments", Json::object()}}, requests, mutex, 4);
  CheckRequest({"command", "f 1", "--port", port},
               {{"schema_version", 1}, {"request_id", "cli-1"},
                {"operation", "command"}, {"value", "f 1"}}, requests, mutex);

  std::size_t requestCount;
  {
    std::lock_guard lock(mutex);
    requestCount = requests.size();
  }
  for (const auto &args : std::vector<std::vector<std::string_view>>{
           {"execute"}, {"execute", "scene.selection.update", "--args"},
           {"execute", "scene.transform.batch", "--port", port},
           {"query", "scene.object.get", "--args", "[1,2]", "--port", port},
           {"query", "scene.object.get", "--args", "null", "--port", port},
           {"query", "scene.object.get", "--args", "{bad}", "--port", port},
           {"query", "scene.object.get", "--args", "{}", "--args", "{}", "--port", port},
           {"query", "scene.summary", "--port", port, "--port", port},
           {"query", "scene.summary", "--port", "0"},
           {"query", "scene.summary", "--port", "65536"},
           {"query", "scene.summary", "--port", "12invalid"},
           {"query", "scene.summary", "--unknown", "{}", "--port", port},
           {"command", "clear", "--args", "{}", "--port", port}}) {
    std::ostringstream out;
    std::ostringstream err;
    assert(perastage::cli::RunLive(args, out, err) == 2);
    assert(out.str().empty() && !err.str().empty());
  }
  {
    std::lock_guard lock(mutex);
    assert(requests.size() == requestCount);
  }
  server.Stop();
  return 0;
}
