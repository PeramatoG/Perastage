#pragma once

#include "inspection/inspection_contract.h"

#include "json.hpp"

namespace perastage::inspection::serialization::detail {

// Serializes one diagnostic through the shared private machine-token mapping.
nlohmann::json SerializeDiagnostic(const Diagnostic &diagnostic);

// Builds the shared backward-compatible base JSON object for inspection
// reports.
nlohmann::json SerializeBase(const Result &result);

} // namespace perastage::inspection::serialization::detail
