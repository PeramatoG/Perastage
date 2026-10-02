#pragma once

#include "command/command_execution.h"
#include "scene_grouping.h"

#include <string>

namespace perastage::live {

struct ExecutionResult {
  std::string response;
  command::MutationSummary mutation;
};

// Executes one local-live wire request against an explicitly supplied context.
ExecutionResult ExecuteRequest(
    const std::string &wireRequest, command::ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &transformPolicy);

} // namespace perastage::live
