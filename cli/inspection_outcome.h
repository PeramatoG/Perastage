#pragma once

#include "inspection/inspection_contract.h"
#include "inspection/xml_schema_validation.h"

#include <span>

namespace perastage::cli {

// Classifies an inspection result into the public process exit-code contract.
int ClassifyInspectionOutcome(
    const inspection::Result &result,
    std::span<const inspection::ValidationResult> validation,
    bool structuredOutputAvailable);

} // namespace perastage::cli
