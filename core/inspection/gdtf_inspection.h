#pragma once

#include "gdtf/editor/gdtf_document.h"
#include "inspection/package_inspection.h"

#include <cstdint>
#include <filesystem>
#include <optional>

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
  GdtfReadStatus status = GdtfReadStatus::Unusable;

  bool Success() const;
};

GdtfInspectionResult InspectGdtf(const Request &request);
GdtfInspectionResult InspectGdtf(const std::filesystem::path &sourcePath);

} // namespace perastage::inspection
