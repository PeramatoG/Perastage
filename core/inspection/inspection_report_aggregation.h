#pragma once

#include "inspection/inspection_contract.h"
#include "inspection/xml_schema_validation.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace perastage::inspection {

// Summarizes operational availability and findings across a complete report.
struct ReportAggregate {
  bool operationalSuccess = false;
  bool hasFindings = false;
  std::optional<DiagnosticSeverity> worstSeverity;
  std::array<std::size_t, 4> uniqueDiagnosticCounts{};
};

ReportAggregate AggregateReport(const Result &inspection,
                                std::span<const ValidationResult> validation,
                                bool structuredOutputAvailable);

std::vector<Diagnostic>
CollectUniqueDiagnostics(const Result &inspection,
                         std::span<const ValidationResult> validation);

} // namespace perastage::inspection
