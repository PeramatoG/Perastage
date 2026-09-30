#pragma once

#include "command/command_selection.h"
#include "command/command_text_parser.h"

namespace perastage::command::text {

// Resolves Console numeric selection syntax and executes stable UUID semantics.
Result ExecuteSelection(const SelectionCommand &command,
                        ExecutionContext &context);

} // namespace perastage::command::text
