#include "inspector_project_source.h"

#include "guiconfigservices.h"
#include "mvrexporter.h"

#include <cstdlib>

namespace gui::inspection {

// Stores the explicitly injected project-session dependency.
CurrentProjectInspector::CurrentProjectInspector(
    const IGuiProjectSessionService &project,
    const IGuiPreferencesService &preferences)
    : project_(project), preferences_(preferences) {}

// Copies live scene state and captures exporter behavior on the GUI thread.
CurrentProjectSnapshotInput CurrentProjectInspector::CaptureInput() const {
  CurrentProjectSnapshotInput input;
  input.scene = std::make_shared<const MvrScene>(project_.GetScene());
  const auto authority = preferences_.GetValue("mvr_truss_geometry_authority");
  input.options = CanonicalMvrExportOptions();
  input.options.trussGeometryAuthority =
      authority && std::strtof(authority->c_str(), nullptr) >= 0.5f
          ? MvrTrussGeometryAuthority::Gdtf
          : MvrTrussGeometryAuthority::MvrGeometry;
  return input;
}

// Serializes a previously captured immutable scene without GUI/global access.
std::optional<std::vector<std::uint8_t>> CurrentProjectInspector::Serialize(
    const CurrentProjectSnapshotInput &input) {
  std::vector<std::uint8_t> bytes;
  MvrExporter exporter;
  if (!input.scene || !exporter.ExportCanonicalSnapshotToBuffer(
                          *input.scene, bytes, input.options))
    return std::nullopt;
  return bytes;
}

// Generates and inspects a canonical in-memory snapshot without mutating state.
std::optional<CurrentProjectInspection> CurrentProjectInspector::Capture() const {
  const auto bytes = Serialize(CaptureInput());
  if (!bytes)
    return std::nullopt;
  CurrentProjectInspection captured;
  captured.bytes = *bytes;
  captured.result = perastage::inspection::InspectMvrBytes(captured.bytes);
  return captured;
}

} // namespace gui::inspection
