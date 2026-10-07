#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace perastage::cli {

// Runs one text command, typed semantic execution, or query against local live IPC.
int RunLive(std::span<const std::string_view> args, std::ostream &out,
            std::ostream &err);

} // namespace perastage::cli
