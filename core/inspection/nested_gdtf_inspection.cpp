#include "inspection/nested_gdtf_inspection.h"

namespace perastage::inspection {
namespace {
// Diagnoses a selected entry whose bounded signature is not a GDTF package.
void AddInvalidNestedGdtf(ResourceReadResult &resource) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Error;
  diagnostic.domain = DiagnosticDomain::Content;
  diagnostic.code = resource_diagnostic_codes::InvalidNestedGdtf;
  diagnostic.message =
      "The selected MVR resource is not a recognizable GDTF package.";
  diagnostic.location = DiagnosticLocation{
      resource.inspection.request.sourcePath, resource.resolvedPath};
  resource.inspection.diagnostics.push_back(std::move(diagnostic));
}
} // namespace

// Reports whether bounded acquisition and normal GDTF inspection succeeded.
bool NestedGdtfInspectionResult::Success() const {
  return resource.Success() && gdtf && gdtf->Success();
}

// Opens one embedded filesystem MVR entry through the normal GDTF inspector.
NestedGdtfInspectionResult
InspectNestedGdtf(const std::filesystem::path &mvrPath,
                  const std::string &entryPath, std::uint64_t maxPackageBytes) {
  NestedGdtfInspectionResult result;
  result.resource = ReadPackageResource(mvrPath, PackageKind::Mvr, entryPath,
                                        maxPackageBytes);
  if (result.resource.Success() &&
      result.resource.kind == ResourceKind::NestedGdtf)
    result.gdtf = InspectGdtf(result.resource.bytes, Request{mvrPath});
  else if (result.resource.Success())
    AddInvalidNestedGdtf(result.resource);
  return result;
}

// Opens one embedded owned-buffer MVR entry through the normal GDTF inspector.
NestedGdtfInspectionResult
InspectNestedGdtf(std::span<const std::uint8_t> mvrBytes,
                  const std::string &entryPath, std::uint64_t maxPackageBytes,
                  const Request &request) {
  NestedGdtfInspectionResult result;
  result.resource = ReadPackageResource(mvrBytes, PackageKind::Mvr, entryPath,
                                        maxPackageBytes, request);
  if (result.resource.Success() &&
      result.resource.kind == ResourceKind::NestedGdtf)
    result.gdtf = InspectGdtf(result.resource.bytes, request);
  else if (result.resource.Success())
    AddInvalidNestedGdtf(result.resource);
  return result;
}

} // namespace perastage::inspection
