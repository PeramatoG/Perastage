#include "command/command_contract.h"

#include <cassert>

using namespace perastage::command;

// Verifies stable identifier and result semantics without a frontend.
int main() {
  assert(IsValidCommandId("scene.transform.position"));
  assert(IsValidCommandId("scene2.select"));
  assert(!IsValidCommandId("Scene.transform"));
  assert(!IsValidCommandId("scene..transform"));
  assert(!IsValidCommandId("scene_transform"));
  assert(!IsValidCommandId("2scene.transform"));

  assert(IsValidArgumentId("fixture_ids"));
  assert(IsValidArgumentId("axis2"));
  assert(!IsValidArgumentId("Fixture_ids"));
  assert(!IsValidArgumentId("fixture.id"));
  assert(!IsValidArgumentId("_fixture"));
  assert(!IsValidArgumentId("fixture__ids"));
  assert(!IsValidArgumentId("2fixtures"));

  assert(IsValidDiagnosticCode("command.argument.invalid"));
  assert(IsValidDiagnosticCode("command.argument.invalid_type"));
  assert(IsValidDiagnosticCode("scene.transform.no_selection"));
  assert(IsValidDiagnosticCode("package.open_failed"));
  assert(IsValidDiagnosticCode("mvr.xml.parse_failed"));
  assert(!IsValidDiagnosticCode("Command.argument.invalid"));
  assert(!IsValidDiagnosticCode("command..invalid"));
  assert(!IsValidDiagnosticCode("command_argument_invalid"));
  assert(!IsValidDiagnosticCode("command.argument._invalid"));
  assert(!IsValidDiagnosticCode("command.argument.invalid_"));
  assert(!IsValidDiagnosticCode("command.argument.invalid__type"));

  Result success{Request{"test.command", {}}, Outcome::Success, {}, {}};
  assert(success.Success());
  assert(!success.HasWarnings());
  assert(!success.HasErrors());

  success.diagnostics.push_back(
      {DiagnosticSeverity::Warning, DiagnosticPhase::Execution,
       "command.test.warning", "Test warning.", std::nullopt});
  assert(success.Success());
  assert(success.HasWarnings());

  Result parseFailure{std::nullopt, Outcome::ParseError, {}, {}};
  Result validationFailure{
      Request{"test.command", {}}, Outcome::ValidationError, {}, {}};
  Result executionFailure{
      Request{"test.command", {}}, Outcome::ExecutionError, {}, {}};
  assert(!parseFailure.request.has_value());
  assert(parseFailure.outcome != validationFailure.outcome);
  assert(validationFailure.outcome != executionFailure.outcome);

  MutationSummary mutation{true, false, false, true, true};
  assert(mutation.HasSemanticChanges());
  assert(!MutationSummary{}.HasSemanticChanges());
  return 0;
}
