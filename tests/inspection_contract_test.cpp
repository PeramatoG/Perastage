#include "inspection/inspection_contract.h"

#include <cassert>
#include <string>

using namespace perastage::inspection;

// Characterizes the GUI-independent read-only inspection contract.
int main() {
  const Request request{std::filesystem::path("fixtures/example.gdtf")};
  Result result{request, {}};

  assert(result.request.sourcePath == request.sourcePath);
  assert(result.Success());
  assert(!result.HasFatalDiagnostics());
  assert(!result.WorstSeverity().has_value());

  Diagnostic compatibilityWarning;
  compatibilityWarning.severity = DiagnosticSeverity::Warning;
  compatibilityWarning.domain = DiagnosticDomain::Package;
  compatibilityWarning.classification =
      DiagnosticClassification::Compatibility;
  compatibilityWarning.code = "package.legacy_filename_encoding";
  compatibilityWarning.message =
      "A legacy filename encoding was used while reading the package.";
  result.diagnostics.push_back(compatibilityWarning);

  assert(result.Success());
  assert(!result.HasFatalDiagnostics());
  assert(result.WorstSeverity() == DiagnosticSeverity::Warning);
  assert(result.diagnostics.front().code ==
         "package.legacy_filename_encoding");
  assert(!result.diagnostics.front().location.has_value());

  Diagnostic standardsError;
  standardsError.severity = DiagnosticSeverity::Error;
  standardsError.domain = DiagnosticDomain::Xml;
  standardsError.classification = DiagnosticClassification::Standards;
  standardsError.code = "xml.required_element_missing";
  standardsError.message = "A required XML element is missing.";
  standardsError.location = DiagnosticLocation{
      request.sourcePath, "description.xml", "/GDTF/FixtureType",
      std::uint32_t{12}, std::uint32_t{4}};
  result.diagnostics.push_back(standardsError);

  assert(result.Success());
  assert(result.diagnostics[0].classification ==
         DiagnosticClassification::Compatibility);
  assert(result.diagnostics[1].classification ==
         DiagnosticClassification::Standards);
  assert(result.diagnostics[1].location->packageEntry == "description.xml");
  assert(result.diagnostics[1].location->line == 12);
  assert(result.WorstSeverity() == DiagnosticSeverity::Error);

  Diagnostic fatalReadFailure;
  fatalReadFailure.severity = DiagnosticSeverity::Fatal;
  fatalReadFailure.domain = DiagnosticDomain::Input;
  fatalReadFailure.code = "input.open_failed";
  fatalReadFailure.message = "The input file could not be opened.";
  result.diagnostics.push_back(fatalReadFailure);

  assert(!result.Success());
  assert(result.HasFatalDiagnostics());
  assert(result.WorstSeverity() == DiagnosticSeverity::Fatal);
  assert(result.diagnostics[0].code == "package.legacy_filename_encoding");
  assert(result.diagnostics[1].code == "xml.required_element_missing");
  assert(result.diagnostics[2].code == "input.open_failed");
}
