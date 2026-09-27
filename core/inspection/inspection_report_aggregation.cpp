#include "inspection/inspection_report_aggregation.h"

#include <algorithm>

namespace perastage::inspection {
namespace {

// Compares optional diagnostic locations without discarding source context.
bool SameLocation(const std::optional<DiagnosticLocation> &left,
                  const std::optional<DiagnosticLocation> &right) {
  if (left.has_value() != right.has_value())
    return false;
  if (!left)
    return true;
  return left->sourcePath == right->sourcePath &&
         left->packageEntry == right->packageEntry &&
         left->xmlPath == right->xmlPath && left->line == right->line &&
         left->column == right->column;
}

// Compares every semantic field used to identify one human-facing finding.
bool SameDiagnostic(const Diagnostic &left, const Diagnostic &right) {
  return left.severity == right.severity && left.domain == right.domain &&
         left.classification == right.classification &&
         left.code == right.code && left.message == right.message &&
         SameLocation(left.location, right.location);
}

// Appends a diagnostic only when its complete semantic identity is new.
void AppendUnique(std::vector<Diagnostic> &diagnostics,
                  const Diagnostic &candidate) {
  if (std::none_of(diagnostics.begin(), diagnostics.end(),
                   [&](const Diagnostic &existing) {
                     return SameDiagnostic(existing, candidate);
                   }))
    diagnostics.push_back(candidate);
}

} // namespace

// Collects diagnostics for human presentation while retaining first-seen order.
std::vector<Diagnostic>
CollectUniqueDiagnostics(const Result &inspection,
                         std::span<const ValidationResult> validation) {
  std::vector<Diagnostic> diagnostics;
  for (const Diagnostic &diagnostic : inspection.diagnostics)
    AppendUnique(diagnostics, diagnostic);
  for (const ValidationResult &layer : validation)
    for (const Diagnostic &diagnostic : layer.diagnostics)
      AppendUnique(diagnostics, diagnostic);
  return diagnostics;
}

// Derives one report-wide outcome without collapsing validation provenance.
ReportAggregate AggregateReport(const Result &inspection,
                                std::span<const ValidationResult> validation,
                                bool structuredOutputAvailable) {
  ReportAggregate aggregate;
  aggregate.operationalSuccess =
      structuredOutputAvailable && inspection.Success();
  for (const Diagnostic &diagnostic :
       CollectUniqueDiagnostics(inspection, validation)) {
    ++aggregate.uniqueDiagnosticCounts[static_cast<std::size_t>(
        diagnostic.severity)];
    if (!aggregate.worstSeverity ||
        diagnostic.severity > *aggregate.worstSeverity)
      aggregate.worstSeverity = diagnostic.severity;
    aggregate.hasFindings =
        aggregate.hasFindings ||
        diagnostic.severity == DiagnosticSeverity::Warning ||
        diagnostic.severity == DiagnosticSeverity::Error ||
        diagnostic.severity == DiagnosticSeverity::Fatal;
  }
  return aggregate;
}

} // namespace perastage::inspection
