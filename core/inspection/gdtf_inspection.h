#pragma once

#include "gdtf/editor/gdtf_document.h"
#include "inspection/package_inspection.h"
#include "inspection/xml_schema_validation.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace perastage::inspection {

// Summarizes whether semantic GDTF data was read canonically or tolerantly.
enum class GdtfReadStatus : std::uint8_t {
  Unusable,
  Canonical,
  CompatibilityAccepted,
};

// Combines package inventory and existing immutable GDTF read snapshots.
struct GdtfInspectionResult {
  Result inspection;
  std::optional<PackageInventory> packageInventory;
  std::optional<gdtf::GdtfDocument> document;
  std::vector<ValidationResult> validation;
  GdtfReadStatus status = GdtfReadStatus::Unusable;

  bool Success() const;
};

GdtfInspectionResult InspectGdtf(const Request &request);
GdtfInspectionResult InspectGdtf(const std::filesystem::path &sourcePath);
GdtfInspectionResult InspectGdtf(const std::vector<std::uint8_t> &bytes,
                                 const Request &request = {});

} // namespace perastage::inspection
