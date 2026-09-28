#pragma once

#include "inspection/mvr_inspection.h"

#include <cstdint>
#include <optional>
#include <vector>

class IGuiProjectSessionService;

namespace gui::inspection {

// Owns the exact canonical project bytes associated with an inspection result.
struct CurrentProjectInspection {
  std::vector<std::uint8_t> bytes;
  perastage::inspection::MvrInspectionResult result;
};

// Generates immutable current-project snapshots through the canonical exporter.
class CurrentProjectInspector final {
public:
  explicit CurrentProjectInspector(const IGuiProjectSessionService &project);

  std::optional<CurrentProjectInspection> Capture() const;

private:
  const IGuiProjectSessionService &project_;
};

} // namespace gui::inspection
