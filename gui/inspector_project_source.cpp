#include "inspector_project_source.h"

#include "guiconfigservices.h"
#include "mvrexporter.h"

namespace gui::inspection {

// Stores the explicitly injected project-session dependency.
CurrentProjectInspector::CurrentProjectInspector(
    const IGuiProjectSessionService &project)
    : project_(project) {}

// Generates and inspects a canonical in-memory snapshot without mutating state.
std::optional<CurrentProjectInspection> CurrentProjectInspector::Capture() const {
  CurrentProjectInspection captured;
  MvrExporter exporter;
  if (!exporter.ExportCanonicalSnapshotToBuffer(project_.GetScene(),
                                                captured.bytes))
    return std::nullopt;
  captured.result = perastage::inspection::InspectMvrBytes(captured.bytes);
  return captured;
}

} // namespace gui::inspection
