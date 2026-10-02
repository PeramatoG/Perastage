#include "local_ipc/local_ipc_contract.h"

#include "json.hpp"

#include <utility>

namespace perastage::local_ipc {
namespace {
using Json = nlohmann::ordered_json;
}

// Creates a deterministic protocol error response.
std::string ErrorResponse(const std::string &requestId, const std::string &code,
                          const std::string &message) {
  Json response = {{"schema_version", 1}, {"request_id", requestId},
                   {"ok", false},
                   {"error", {{"code", code}, {"message", message}}}};
  return response.dump();
}

// Parses and validates the versioned local IPC request contract.
bool ParseRequest(const std::string &json, Request &request,
                  std::string &errorResponse) {
  const Json value = Json::parse(json, nullptr, false);
  if (value.is_discarded() || !value.is_object()) {
    errorResponse = ErrorResponse({}, "invalid_json", "Request is not valid JSON.");
    return false;
  }
  const std::string requestId =
      value.value("request_id", Json()).is_string()
          ? value["request_id"].get<std::string>() : std::string{};
  if (!value.contains("schema_version") || !value["schema_version"].is_number_integer() ||
      value["schema_version"].get<int>() != 1) {
    errorResponse = ErrorResponse(requestId, "unsupported_version",
                                  "Only schema_version 1 is supported.");
    return false;
  }
  if (requestId.empty() || requestId.size() > 128 ||
      !value.value("operation", Json()).is_string() ||
      !value.value("value", Json()).is_string()) {
    errorResponse = ErrorResponse(requestId, "invalid_request",
                                  "request_id, operation, and value must be bounded strings.");
    return false;
  }
  request = {requestId, value["operation"].get<std::string>(),
             value["value"].get<std::string>()};
  if (request.operation.size() > 64 || request.value.size() > 32768) {
    errorResponse = ErrorResponse(requestId, "invalid_request",
                                  "Request fields exceed their size limit.");
    return false;
  }
  return true;
}

// Creates a deterministic successful response with a structured result value.
std::string SuccessResponse(const std::string &requestId,
                            const std::string &resultJson) {
  Json result = Json::parse(resultJson, nullptr, false);
  if (result.is_discarded())
    result = nullptr;
  Json response = {{"schema_version", 1}, {"request_id", requestId},
                   {"ok", true}, {"result", std::move(result)}};
  return response.dump();
}

} // namespace perastage::local_ipc
