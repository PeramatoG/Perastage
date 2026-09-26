#pragma once

#include "gdtf_inspection.h"
#include "inspection_json_serializer.h"
#include "mvr_inspection.h"
#include "resource_inspection.h"

#include <string>
#include <vector>

namespace perastage::inspection::serialization {

// Serializes one complete GDTF report from existing structured results.
std::string
SerializeGdtfReportToJson(const GdtfInspectionResult &result,
                          const std::vector<ResourceDescriptor> &resources);

// Serializes one complete MVR report from existing structured results.
std::string
SerializeMvrReportToJson(const MvrInspectionResult &result,
                         const std::vector<ResourceDescriptor> &resources);

} // namespace perastage::inspection::serialization
