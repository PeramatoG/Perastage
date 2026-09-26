#pragma once

#include "gdtf_inspection.h"
#include "inspection_contract.h"
#include "mvr_inspection.h"
#include "resource_inspection.h"

#include <cstdint>
#include <string>

namespace perastage::inspection::serialization {

inline constexpr std::uint32_t kInspectionJsonSchemaVersion = 1;

// Serializes an inspection result to the compact versioned JSON contract.
std::string SerializeResultToJson(const Result &result);

// Serializes one complete GDTF report from existing structured results.
std::string
SerializeGdtfReportToJson(const GdtfInspectionResult &result,
                          const std::vector<ResourceDescriptor> &resources);

// Serializes one complete MVR report from existing structured results.
std::string
SerializeMvrReportToJson(const MvrInspectionResult &result,
                         const std::vector<ResourceDescriptor> &resources);

} // namespace perastage::inspection::serialization
