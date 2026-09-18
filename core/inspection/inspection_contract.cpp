#include "inspection_contract.h"

#include <algorithm>

namespace perastage::inspection {

// Reports whether any diagnostic marks an unrecoverable inspection failure.
bool Result::HasFatalDiagnostics() const {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic &diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::Fatal;
                     });
}

// Reports success when inspection produced no fatal diagnostic.
bool Result::Success() const { return !HasFatalDiagnostics(); }

// Returns the greatest severity, or no value when there are no diagnostics.
std::optional<DiagnosticSeverity> Result::WorstSeverity() const {
  if (diagnostics.empty())
    return std::nullopt;

  return std::max_element(
             diagnostics.begin(), diagnostics.end(),
             [](const Diagnostic &left, const Diagnostic &right) {
               return left.severity < right.severity;
             })
      ->severity;
}

} // namespace perastage::inspection
