#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspection/mvr_inspection.h"
#include "inspection/resource_inspection.h"

#include <string>
#include <vector>

namespace perastage::cli {

std::string
FormatGdtfSummary(const inspection::GdtfInspectionResult &result,
                  const std::vector<inspection::ResourceDescriptor> &resources);
std::string
FormatMvrSummary(const inspection::MvrInspectionResult &result,
                 const std::vector<inspection::ResourceDescriptor> &resources);
std::string FormatInventory(const inspection::PackageInventory &inventory);
std::string
FormatResources(const std::vector<inspection::ResourceDescriptor> &resources);
std::string
FormatDiagnostics(const inspection::Result &result,
                  const std::vector<inspection::ValidationResult> &validation);

} // namespace perastage::cli
