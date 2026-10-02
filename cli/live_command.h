#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace perastage::cli {

// Runs one command or query against the loopback-only live application endpoint.
int RunLive(std::span<const std::string_view> args, std::ostream &out,
            std::ostream &err);

} // namespace perastage::cli
