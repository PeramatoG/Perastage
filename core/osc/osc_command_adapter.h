#pragma once

#include "command/command_contract.h"
#include "command/command_execution.h"
#include "interactive_transform_policy.h"
#include "osc/osc_message.h"

namespace perastage::osc {

// Maps and executes one supported OSC message through semantic Commands.
command::Result
ExecuteMessage(const Message &message, command::ExecutionContext &context,
               const scene_grouping::InteractiveTransformPolicy &policy);

} // namespace perastage::osc
