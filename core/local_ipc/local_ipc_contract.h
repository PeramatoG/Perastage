#pragma once

#include <string>

namespace perastage::local_ipc {

struct Request {
  std::string requestId;
  std::string operation;
  std::string value;
  // Optional serialized JSON object; an empty string means no arguments.
  std::string argumentsJson;
};

// Parses and validates the versioned local IPC request contract.
bool ParseRequest(const std::string &json, Request &request,
                  std::string &errorResponse);
// Creates a deterministic successful response with a structured result value.
std::string SuccessResponse(const std::string &requestId,
                            const std::string &resultJson);
// Creates a deterministic protocol error response.
std::string ErrorResponse(const std::string &requestId, const std::string &code,
                          const std::string &message);

} // namespace perastage::local_ipc
