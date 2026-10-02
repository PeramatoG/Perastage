#include "live_command.h"

#include "json.hpp"
#include "local_ipc/local_ipc_transport.h"

#include <charconv>
#include <ostream>
#include <string>

namespace perastage::cli {

// Runs one command or query against the loopback-only live application
// endpoint.
int RunLive(std::span<const std::string_view> args, std::ostream &out,
            std::ostream &err) {
  if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
    out << "Usage: perastage-cli live <command <text> | query <id>> [--port "
           "<port>]\n";
    return 0;
  }
  if (args.size() < 2 || (args[0] != "command" && args[0] != "query")) {
    err << "perastage-cli live: expected 'command <text>' or 'query <id>'.\n";
    return 2;
  }
  std::uint16_t port = local_ipc::kDefaultPort;
  if (args.size() != 2) {
    if (args.size() != 4 || args[2] != "--port") {
      err << "perastage-cli live: invalid options.\n";
      return 2;
    }
    unsigned parsed = 0;
    const auto conversion = std::from_chars(
        args[3].data(), args[3].data() + args[3].size(), parsed);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != args[3].data() + args[3].size() || parsed == 0 ||
        parsed > 65535) {
      err << "perastage-cli live: invalid port.\n";
      return 2;
    }
    port = static_cast<std::uint16_t>(parsed);
  }
  const nlohmann::ordered_json request = {{"schema_version", 1},
                                          {"request_id", "cli-1"},
                                          {"operation", std::string(args[0])},
                                          {"value", std::string(args[1])}};
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
  if (args[0] == "command" && parsedResponse.contains("result") &&
      !parsedResponse["result"].value("success", false))
    return 4;
  return 0;
}

} // namespace perastage::cli
