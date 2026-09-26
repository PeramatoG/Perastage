#pragma once

#include "inspection_contract.h"

#include <cstdint>
#include <string>

namespace perastage::inspection::serialization {

inline constexpr std::uint32_t kInspectionJsonSchemaVersion = 1;

// Serializes an inspection result to the compact versioned JSON contract.
std::string SerializeResultToJson(const Result &result);

} // namespace perastage::inspection::serialization
