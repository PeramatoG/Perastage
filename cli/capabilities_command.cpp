#include "capabilities_command.h"

#include "capability/capability_catalog.h"
#include "capability/capability_json_serializer.h"

#include <ostream>

namespace perastage::cli {

// Runs the read-only capability discovery command.
int RunCapabilities(std::span<const std::string_view> args, std::ostream &out,
                    std::ostream &err) {
  if (args.size() == 1 && (args.front() == "-h" || args.front() == "--help")) {
    out << "Usage: perastage-cli capabilities [--json]\n\n"
           "Options:\n"
           "  --json      Emit discovery schema version 1.\n"
           "  -h, --help  Show this help and exit.\n";
    return 0;
  }
  if (args.size() > 1 || (!args.empty() && args.front() != "--json")) {
    err << "perastage-cli capabilities: unknown option: "
        << (args.empty() ? "" : args.front())
        << "\nTry 'perastage-cli capabilities --help' for usage.\n";
    return 2;
  }
  if (!args.empty()) {
    out << capability::SerializeCatalogJson();
    return 0;
  }
  out << "Semantic capabilities and current frontend exposure:\n";
  for (const capability::Descriptor &descriptor : capability::Catalog()) {
    out << descriptor.operationId << "  " << capability::Token(descriptor.kind)
        << "  " << capability::Token(descriptor.effect) << "\n  "
        << descriptor.summary << "\n  frontends: ";
    if (descriptor.frontends.empty())
      out << "none";
    for (std::size_t index = 0; index < descriptor.frontends.size(); ++index) {
      if (index != 0)
        out << ", ";
      out << descriptor.frontends[index].frontendId << " ("
          << capability::Token(descriptor.frontends[index].state) << ')';
    }
    out << '\n';
  }
  return 0;
}

} // namespace perastage::cli
