#include "local_ipc/local_ipc_contract.h"
#include "local_ipc/local_ipc_transport.h"
#include "json.hpp"

#include <cassert>
#include <string>

// Verifies contract errors, lifecycle, loopback exchange, and shared handler state.
int main() {
  using namespace perastage::local_ipc;
  Request parsed;
  std::string errorResponse;
  assert(!ParseRequest("{}", parsed, errorResponse));
  assert(nlohmann::json::parse(errorResponse)["error"]["code"] ==
         "unsupported_version");

  int liveValue = 0;
  Server server;
  std::string error;
  assert(server.Start(
      0,
      [&](const std::string &wire) {
        Request request;
        std::string parseError;
        if (!ParseRequest(wire, request, parseError))
          return parseError;
        if (request.operation == "command") {
          liveValue = std::stoi(request.value);
          return SuccessResponse(request.requestId, R"({"changed":true})");
        }
        if (request.operation == "query")
          return SuccessResponse(request.requestId,
                                 "{\"value\":" + std::to_string(liveValue) + "}");
        return ErrorResponse(request.requestId, "unsupported_operation", "Unsupported.");
      }, error));
  assert(server.Port() != 0);

  std::string response;
  assert(Exchange(server.Port(),
                  R"({"schema_version":1,"request_id":"mutate","operation":"command","value":"42"})",
                  response, error));
  assert(nlohmann::json::parse(response)["ok"] == true);
  assert(Exchange(server.Port(),
                  R"({"schema_version":1,"request_id":"query","operation":"query","value":"state"})",
                  response, error));
  assert(nlohmann::json::parse(response)["result"]["value"] == 42);
  server.Stop();
  return 0;
}
