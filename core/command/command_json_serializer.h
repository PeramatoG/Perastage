#pragma once

#include "command/command_contract.h"

#include <string>

namespace perastage::command::serialization {

inline constexpr int kCommandJsonSchemaVersion = 1;

std::string SerializeResultToJson(const Result &result);

} // namespace perastage::command::serialization
