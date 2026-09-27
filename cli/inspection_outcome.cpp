#include "inspection_outcome.h"

#include "inspection/inspection_report_aggregation.h"

namespace perastage::cli {

// Classifies diagnostics without consulting presentation strings.
int ClassifyInspectionOutcome(
    const inspection::Result &result,
    std::span<const inspection::ValidationResult> validation,
    bool structuredOutputAvailable) {
  const inspection::ReportAggregate aggregate = inspection::AggregateReport(
      result, validation, structuredOutputAvailable);
  if (!aggregate.operationalSuccess ||
      aggregate.worstSeverity == inspection::DiagnosticSeverity::Fatal) {
    return 3;
  }
  return aggregate.hasFindings ? 1 : 0;
}

} // namespace perastage::cli
