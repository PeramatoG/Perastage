#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace perastage::cli {

// Runs the explicit file-backed headless scene mutation workflow.
int RunScene(std::span<const std::string_view> args, std::ostream &out,
             std::ostream &err);

} // namespace perastage::cli
