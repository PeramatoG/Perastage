#pragma once

#include "command/command_execution.h"
#include "command/command_operation_ids.h"

namespace perastage::command::transform {

// Executes aligned explicit-target component lists as one atomic mutation.
Result ExecuteBatch(const Request &request, ExecutionContext &context);

} // namespace perastage::command::transform
