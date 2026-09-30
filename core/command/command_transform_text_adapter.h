#pragma once

#include "command/command_text_parser.h"
#include "command/command_transform.h"

namespace perastage::command::text {

// Converts neutral Console transform syntax to explicit semantic units.
transform::Command AdaptTransform(const TransformCommand &command);

} // namespace perastage::command::text
