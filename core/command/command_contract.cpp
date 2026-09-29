#include "command_contract.h"

#include <algorithm>
#include <stdexcept>

namespace perastage::command {
namespace {

// Returns whether a segment follows the selected lowercase ASCII grammar.
bool IsLowercaseAsciiSegment(const std::string &segment, bool allowUnderscore,
                             bool requireLeadingLetter) {
  if (segment.empty() || (requireLeadingLetter &&
                          (segment.front() < 'a' || segment.front() > 'z')))
    return false;
  return std::all_of(segment.begin(), segment.end(), [allowUnderscore](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           (allowUnderscore && c == '_');
  });
}

// Validates a dotted identifier with the selected lowercase segment grammar.
bool IsValidDottedId(const std::string &id, bool allowUnderscore,
                     bool requireLeadingLetter) {
  if (id.empty())
    return false;
  std::size_t start = 0;
  while (start < id.size()) {
    const std::size_t end = id.find('.', start);
    const std::string segment = id.substr(start, end - start);
    if (!IsLowercaseAsciiSegment(segment, allowUnderscore,
                                 requireLeadingLetter) ||
        (allowUnderscore && (segment.front() == '_' || segment.back() == '_' ||
                             segment.find("__") != std::string::npos)))
      return false;
    if (end == std::string::npos)
      return true;
    start = end + 1;
  }
  return false;
}

} // namespace

// Returns whether the summary contains a mutation requiring publication.
bool MutationSummary::HasSemanticChanges() const {
  return sceneChanged || selectionChanged || projectMetadataChanged;
}

// Returns whether command processing completed successfully.
bool Result::Success() const { return outcome == Outcome::Success; }

// Returns whether any ordered diagnostic is a warning.
bool Result::HasWarnings() const {
  return std::any_of(
      diagnostics.begin(), diagnostics.end(), [](const Diagnostic &diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Warning;
      });
}

// Returns whether any ordered diagnostic is an error.
bool Result::HasErrors() const {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic &diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::Error;
                     });
}

// Validates a stable lowercase ASCII dotted command identifier.
bool IsValidCommandId(const std::string &id) {
  return IsValidDottedId(id, false, true);
}

// Validates a stable lowercase ASCII snake-case argument identifier.
bool IsValidArgumentId(const std::string &id) {
  return IsLowercaseAsciiSegment(id, true, true) && id.front() != '_' &&
         id.back() != '_' && id.find("__") == std::string::npos;
}

// Validates a stable lowercase ASCII dotted diagnostic code.
bool IsValidDiagnosticCode(const std::string &code) {
  return code.find('.') != std::string::npos &&
         IsValidDottedId(code, true, false);
}

// Returns the stable machine token for a command outcome.
const char *OutcomeToken(Outcome outcome) {
  switch (outcome) {
  case Outcome::Success:
    return "success";
  case Outcome::ParseError:
    return "parse_error";
  case Outcome::ValidationError:
    return "validation_error";
  case Outcome::ExecutionError:
    return "execution_error";
  }
  throw std::invalid_argument("Unknown command outcome");
}

// Returns the stable machine token for a diagnostic severity.
const char *SeverityToken(DiagnosticSeverity severity) {
  switch (severity) {
  case DiagnosticSeverity::Information:
    return "information";
  case DiagnosticSeverity::Warning:
    return "warning";
  case DiagnosticSeverity::Error:
    return "error";
  }
  throw std::invalid_argument("Unknown command diagnostic severity");
}

// Returns the stable machine token for a diagnostic phase.
const char *PhaseToken(DiagnosticPhase phase) {
  switch (phase) {
  case DiagnosticPhase::Parse:
    return "parse";
  case DiagnosticPhase::Validation:
    return "validation";
  case DiagnosticPhase::Execution:
    return "execution";
  }
  throw std::invalid_argument("Unknown command diagnostic phase");
}

} // namespace perastage::command
