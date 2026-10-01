#pragma once

#include "inspection/mvr_inspection.h"
#include "mvr_export_options.h"
#include "mvr_export_environment.h"
#include "mvrscene.h"

#include <cstdint>
#include <optional>
#include <memory>
#include <vector>

class IGuiProjectSessionService;
class IGuiPreferencesService;

namespace gui::inspection {

// Owns the exact canonical project bytes associated with an inspection result.
struct CurrentProjectInspection {
  std::vector<std::uint8_t> bytes;
  perastage::inspection::MvrInspectionResult result;
};

// Owns a GUI-thread scene copy and explicitly captured export behavior.
struct CurrentProjectSnapshotInput {
  std::shared_ptr<const MvrScene> scene;
  MvrExportOptions options;
  MvrExportEnvironment environment;
};

// Generates immutable current-project snapshots through the canonical exporter.
class CurrentProjectInspector final {
public:
  CurrentProjectInspector(const IGuiProjectSessionService &project,
                          const IGuiPreferencesService &preferences);

  CurrentProjectSnapshotInput CaptureInput() const;
  static std::optional<std::vector<std::uint8_t>> Serialize(
      const CurrentProjectSnapshotInput &input);
  std::optional<CurrentProjectInspection> Capture() const;

private:
  const IGuiProjectSessionService &project_;
  const IGuiPreferencesService &preferences_;
};

} // namespace gui::inspection
