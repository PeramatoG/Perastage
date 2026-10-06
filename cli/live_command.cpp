#include "live_command.h"

#include "json.hpp"
#include "local_ipc/local_ipc_transport.h"

#include <charconv>
#include <ostream>
#include <string>

namespace perastage::cli {

// Runs text commands, semantic commands, or queries through the generic live
// request envelope without acquiring scene logic.
int RunLive(std::span<const std::string_view> args, std::ostream &out,
            std::ostream &err) {
  if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
    out << "Usage: perastage-cli live <command <text> | query <id> | execute "
           "<id>> [--args <json>] [--port <port>]\n";
    return 0;
  }
  if (args.size() < 2 ||
      (args[0] != "command" && args[0] != "query" && args[0] != "execute")) {
    err << "perastage-cli live: expected 'command <text>', 'query <id>', "
           "or 'execute <id>'.\n";
    return 2;
  }
  std::uint16_t port = local_ipc::kDefaultPort;
  bool hasPort = false;
  bool hasArguments = false;
  nlohmann::ordered_json arguments;
  for (std::size_t index = 2; index < args.size(); index += 2) {
    if (index + 1 == args.size()) {
      err << "perastage-cli live: invalid options.\n";
      return 2;
    }
    if (args[index] == "--args" && !hasArguments && args[0] != "command") {
      hasArguments = true;
      arguments = nlohmann::ordered_json::parse(args[index + 1], nullptr, false);
      if (arguments.is_discarded() || !arguments.is_object()) {
        err << "perastage-cli live: --args must be a JSON object.\n";
        return 2;
      }
      continue;
    }
    if (args[index] != "--port" || hasPort) {
      err << "perastage-cli live: invalid options.\n";
      return 2;
    }
    hasPort = true;
    unsigned parsed = 0;
    const auto conversion = std::from_chars(
        args[index + 1].data(),
        args[index + 1].data() + args[index + 1].size(), parsed);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != args[index + 1].data() + args[index + 1].size() ||
        parsed == 0 || parsed > 65535) {
      err << "perastage-cli live: invalid port.\n";
      return 2;
    }
    port = static_cast<std::uint16_t>(parsed);
  }
  if (args[0] == "execute" && !hasArguments) {
    err << "perastage-cli live: execute requires --args <json>.\n";
    return 2;
  }
  nlohmann::ordered_json request = {{"schema_version", 1},
                                    {"request_id", "cli-1"},
                                    {"operation", std::string(args[0])},
                                    {"value", std::string(args[1])}};
  if (hasArguments)
    request["arguments"] = std::move(arguments);
  std::string response;
  std::string error;
  if (!local_ipc::Exchange(port, request.dump(), response, error)) {
    err << "perastage-cli live: " << error << ".\n";
    return 4;
  }
  out << response << '\n';
  const auto parsedResponse = nlohmann::json::parse(response, nullptr, false);
  if (parsedResponse.is_discarded() || !parsedResponse.value("ok", false))
    return 4;
  if ((args[0] == "command" || args[0] == "execute") &&
      parsedResponse.contains("result") &&
      !parsedResponse["result"].value("success", false))
    return 4;
  return 0;
}

} // namespace perastage::cli
