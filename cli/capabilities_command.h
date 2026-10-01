#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace perastage::cli {

// Runs the read-only capability discovery command.
int RunCapabilities(std::span<const std::string_view> args, std::ostream &out,
                    std::ostream &err);

} // namespace perastage::cli
