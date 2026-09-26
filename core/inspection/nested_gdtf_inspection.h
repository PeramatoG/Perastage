#pragma once

#include "inspection/gdtf_inspection.h"
#include "inspection/resource_inspection.h"

#include <optional>
#include <span>

namespace perastage::inspection {

// Combines the bounded MVR entry read with normal GDTF inspection output.
struct NestedGdtfInspectionResult {
  ResourceReadResult resource;
  std::optional<GdtfInspectionResult> gdtf;

  bool Success() const;
};

NestedGdtfInspectionResult
InspectNestedGdtf(const std::filesystem::path &mvrPath,
                  const std::string &entryPath, std::uint64_t maxPackageBytes);
NestedGdtfInspectionResult
InspectNestedGdtf(std::span<const std::uint8_t> mvrBytes,
                  const std::string &entryPath, std::uint64_t maxPackageBytes,
                  const Request &request = {});

} // namespace perastage::inspection
