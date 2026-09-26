#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace perastage::cli {

// Runs the inspect subcommand and routes requested data to standard output.
int RunInspect(std::span<const std::string_view> args, std::ostream &out,
               std::ostream &err);

// Writes the inspect-specific command help.
void WriteInspectHelp(std::ostream &out);

} // namespace perastage::cli
