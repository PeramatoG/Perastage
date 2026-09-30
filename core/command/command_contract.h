#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace perastage::command {

using ArgumentValue =
    std::variant<bool, std::int64_t, double, std::string,
                 std::vector<std::int64_t>, std::vector<double>,
                 std::vector<std::string>>;

struct Argument {
  std::string id;
  ArgumentValue value;
};

struct Request {
  std::string commandId;
  std::vector<Argument> arguments;
};

enum class Outcome { Success, ParseError, ValidationError, ExecutionError };
enum class DiagnosticSeverity { Information, Warning, Error };
enum class DiagnosticPhase { Parse, Validation, Execution };

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Information;
  DiagnosticPhase phase = DiagnosticPhase::Execution;
  std::string code;
  std::string message;
  std::optional<std::string> argumentId;
};

struct MutationSummary {
  bool sceneChanged = false;
  bool selectionChanged = false;
  bool projectMetadataChanged = false;
  bool undoEntryRecorded = false;
  bool projectDirty = false;

  bool HasSemanticChanges() const;
};

struct Result {
  std::optional<Request> request;
  Outcome outcome = Outcome::Success;
  std::vector<Diagnostic> diagnostics;
  MutationSummary mutation;
  std::vector<Argument> outputs;

  bool Success() const;
  bool HasWarnings() const;
  bool HasErrors() const;
};

bool IsValidCommandId(const std::string &id);
bool IsValidArgumentId(const std::string &id);
bool IsValidDiagnosticCode(const std::string &code);

const char *OutcomeToken(Outcome outcome);
const char *SeverityToken(DiagnosticSeverity severity);
const char *PhaseToken(DiagnosticPhase phase);

} // namespace perastage::command
