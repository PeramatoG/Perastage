#include "mvrexporter.h"

#include "configmanager.h"

namespace {

// Applies the interactive application's configured truss authority.
MvrExportOptions ApplicationOptions(MvrExportOptions options) {
  if (!options.trussGeometryAuthority) {
    const float value =
        ConfigManager::Get().GetFloat("mvr_truss_geometry_authority");
    options.trussGeometryAuthority =
        value >= 0.5f ? MvrTrussGeometryAuthority::Gdtf
                      : MvrTrussGeometryAuthority::MvrGeometry;
  }
  return options;
}

} // namespace

// Exports the active application scene with canonical application policy.
bool MvrExporter::ExportToFile(const std::string &filePath) {
  return ExportToFile(filePath, CanonicalMvrExportOptions());
}

// Exports the active application scene with explicit application policy.
bool MvrExporter::ExportToFile(const std::string &filePath,
                               const MvrExportOptions &options) {
  return ExportCanonicalSnapshotToFile(ConfigManager::Get().GetScene(), filePath,
                                       ApplicationOptions(options));
}

// Exports the active application scene to an in-memory archive.
bool MvrExporter::ExportToBuffer(std::vector<uint8_t> &outBytes) {
  return ExportToBuffer(outBytes, CanonicalMvrExportOptions());
}

// Exports the active application scene to memory with explicit policy.
bool MvrExporter::ExportToBuffer(std::vector<uint8_t> &outBytes,
                                 const MvrExportOptions &options) {
  return ExportCanonicalSnapshotToBuffer(ConfigManager::Get().GetScene(),
                                         outBytes, ApplicationOptions(options));
}
