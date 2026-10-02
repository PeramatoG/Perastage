#include "mvrexporter.h"

#include "configmanager.h"
#include "mvr_export_application_environment.h"
#include "projectutils.h"

#include <array>
#include <filesystem>

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

// Resolves the established application fixture fallback in preference order.
MvrExportEnvironment ResolveApplicationMvrExportEnvironment() {
  const std::filesystem::path base =
      ProjectUtils::GetBaseLibraryPath("fixtures");
  const std::array<std::filesystem::path, 5> candidates = {
      base / "Dummy 1ch.gdtf",
      base / "Perastage@Dummy_1ch@Perastage.gdtf",
      base / "Unknown@Dummy_1ch@Perastage.gdtf",
      base / "Generic 1ch.gdtf",
      base / "Generic@Generic_1ch@Perastage.gdtf"};
  for (const auto &candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error) && !error)
      return {.fixtureFallbackGdtfPath = candidate};
  }
  return {};
}

// Exports the active application scene with canonical application policy.
bool MvrExporter::ExportToFile(const std::string &filePath) {
  return ExportToFile(filePath, CanonicalMvrExportOptions());
}

// Exports the active application scene with explicit application policy.
bool MvrExporter::ExportToFile(const std::string &filePath,
                               const MvrExportOptions &options) {
  return ExportCanonicalSnapshotToFile(ConfigManager::Get().GetScene(), filePath,
                                       ApplicationOptions(options),
                                       ResolveApplicationMvrExportEnvironment());
}

// Exports the active application scene to an in-memory archive.
bool MvrExporter::ExportToBuffer(std::vector<uint8_t> &outBytes) {
  return ExportToBuffer(outBytes, CanonicalMvrExportOptions());
}

// Exports the active application scene to memory with explicit policy.
bool MvrExporter::ExportToBuffer(std::vector<uint8_t> &outBytes,
                                 const MvrExportOptions &options) {
  return ExportCanonicalSnapshotToBuffer(ConfigManager::Get().GetScene(),
                                         outBytes, ApplicationOptions(options),
                                         ResolveApplicationMvrExportEnvironment());
}
