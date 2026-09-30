#include "command_selection_text_adapter.h"

#include "mvrscene.h"

#include <algorithm>
#include <string>
#include <vector>

namespace perastage::command::text {
namespace {

// Resolves one numeric Console identifier to a unique, stable UUID.
std::vector<std::string> Resolve(const MvrScene &scene, SelectionTarget target,
                                 int id) {
  std::vector<std::string> matches;
  if (target == SelectionTarget::Fixtures) {
    for (const auto &[uuid, fixture] : scene.fixtures)
      if (fixture.fixtureId == id)
        matches.push_back(uuid);
  } else {
    for (const auto &[uuid, truss] : scene.trusses)
      if (truss.unitNumber == id)
        matches.push_back(uuid);
  }
  std::sort(matches.begin(), matches.end());
  return matches;
}

} // namespace

// Resolves Console numeric selection syntax and executes stable UUID semantics.
Result ExecuteSelection(const SelectionCommand &command,
                        ExecutionContext &context) {
  selection::Command semantic;
  semantic.target = command.target == SelectionTarget::Fixtures
                        ? selection::ObjectKind::Fixture
                        : selection::ObjectKind::Truss;
  semantic.preserveExisting = command.target == SelectionTarget::Fixtures;
  std::vector<Diagnostic> resolutionDiagnostics;
  for (const SelectionOperation &parsed : command.operations) {
    selection::Operation operation;
    operation.kind = parsed.kind == SelectionOperationKind::Add
                         ? selection::OperationKind::Add
                         : selection::OperationKind::Remove;
    for (int id = parsed.firstId; id <= parsed.lastId; ++id) {
      const auto matches = Resolve(context.scene, command.target, id);
      if (matches.empty()) {
        resolutionDiagnostics.push_back(
            {DiagnosticSeverity::Information, DiagnosticPhase::Validation,
             "scene.selection.numeric_id_missing",
             "No scene object matches numeric id " + std::to_string(id) + ".",
             "numeric_ids"});
      } else if (matches.size() > 1) {
        resolutionDiagnostics.push_back(
            {DiagnosticSeverity::Warning, DiagnosticPhase::Validation,
             "scene.selection.numeric_id_ambiguous",
             "Numeric id " + std::to_string(id) +
                 " is ambiguous and was not selected.",
             "numeric_ids"});
      } else {
        operation.objects.push_back({semantic.target, matches.front()});
      }
      if (id == parsed.lastId)
        break;
    }
    semantic.operations.push_back(std::move(operation));
  }
  Result result = selection::Execute(semantic, context);
  result.diagnostics.insert(result.diagnostics.begin(),
                            resolutionDiagnostics.begin(),
                            resolutionDiagnostics.end());
  return result;
}

} // namespace perastage::command::text
