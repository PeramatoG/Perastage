#include "inspector_project_source.h"

#include "guiconfigservices.h"
#include "mvrexporter.h"

namespace gui::inspection {

// Stores the explicitly injected project-session dependency.
CurrentProjectInspector::CurrentProjectInspector(
    const IGuiProjectSessionService &project)
    : project_(project) {}

// Generates immutable canonical bytes while the GUI owns the live scene.
std::optional<std::vector<std::uint8_t>>
CurrentProjectInspector::CaptureBytes() const {
  std::vector<std::uint8_t> bytes;
  MvrExporter exporter;
  if (!exporter.ExportCanonicalSnapshotToBuffer(project_.GetScene(), bytes))
    return std::nullopt;
  return bytes;
}

// Generates and inspects a canonical in-memory snapshot without mutating state.
std::optional<CurrentProjectInspection> CurrentProjectInspector::Capture() const {
  const auto bytes = CaptureBytes();
  if (!bytes)
    return std::nullopt;
  CurrentProjectInspection captured;
  captured.bytes = *bytes;
  captured.result = perastage::inspection::InspectMvrBytes(captured.bytes);
  return captured;
}

} // namespace gui::inspection
