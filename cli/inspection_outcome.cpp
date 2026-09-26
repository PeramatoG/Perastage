#include "inspection_outcome.h"

namespace perastage::cli {
namespace {

// Updates accumulated diagnostic state from one structured finding.
void IncludeDiagnostic(const inspection::Diagnostic &diagnostic, bool &fatal,
                       bool &finding) {
  fatal = fatal || diagnostic.severity == inspection::DiagnosticSeverity::Fatal;
  finding = finding ||
            diagnostic.severity == inspection::DiagnosticSeverity::Warning ||
            diagnostic.severity == inspection::DiagnosticSeverity::Error;
}

} // namespace

// Classifies diagnostics without consulting presentation strings.
int ClassifyInspectionOutcome(
    const inspection::Result &result,
    std::span<const inspection::ValidationResult> validation,
    bool structuredOutputAvailable) {
  bool fatal = false;
  bool finding = false;
  for (const auto &diagnostic : result.diagnostics) {
    IncludeDiagnostic(diagnostic, fatal, finding);
  }
  for (const auto &layer : validation) {
    for (const auto &diagnostic : layer.diagnostics) {
      IncludeDiagnostic(diagnostic, fatal, finding);
    }
  }
  if (fatal || !structuredOutputAvailable) {
    return 3;
  }
  return finding ? 1 : 0;
}

} // namespace perastage::cli
