#include "command_selection_text_adapter.h"

#include "mvrscene.h"

#include <algorithm>
#include <string>
#include <vector>

namespace perastage::command::text {
namespace {

// Resolves one numeric Console identifier to ordered stable UUIDs.
std::vector<std::string> ResolveScene(const MvrScene &scene,
                                      SelectionTarget target, int id) {
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

// Resolves a numeric identifier only among the current ordered selection.
std::vector<std::string>
ResolveSelected(const MvrScene &scene, SelectionTarget target, int id,
                const std::vector<std::string> &selected) {
  std::vector<std::string> matches;
  for (const std::string &uuid : selected) {
    if (target == SelectionTarget::Fixtures) {
      const auto found = scene.fixtures.find(uuid);
      if (found != scene.fixtures.end() && found->second.fixtureId == id)
        matches.push_back(uuid);
    } else {
      const auto found = scene.trusses.find(uuid);
      if (found != scene.trusses.end() && found->second.unitNumber == id)
        matches.push_back(uuid);
    }
  }
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
  std::vector<std::string> resolvedSelection =
      semantic.preserveExisting
          ? (command.target == SelectionTarget::Fixtures
                 ? context.selection.fixtures
                 : context.selection.trusses)
          : std::vector<std::string>{};
  std::vector<Diagnostic> resolutionDiagnostics;
  for (const SelectionOperation &parsed : command.operations) {
    selection::Operation operation;
    operation.kind = parsed.kind == SelectionOperationKind::Add
                         ? selection::OperationKind::Add
                         : selection::OperationKind::Remove;
    for (int id = parsed.firstId; id <= parsed.lastId; ++id) {
      const auto sceneMatches = ResolveScene(context.scene, command.target, id);
      if (sceneMatches.empty()) {
        resolutionDiagnostics.push_back(
            {DiagnosticSeverity::Information, DiagnosticPhase::Validation,
             "scene.selection.numeric_id_missing",
             "No scene object matches numeric id " + std::to_string(id) + ".",
             "numeric_ids"});
      } else if (parsed.kind == SelectionOperationKind::Add &&
                 sceneMatches.size() > 1) {
        resolutionDiagnostics.push_back(
            {DiagnosticSeverity::Warning, DiagnosticPhase::Validation,
             "scene.selection.numeric_id_ambiguous",
             "Numeric id " + std::to_string(id) +
                 " is ambiguous and was not selected.",
             "numeric_ids"});
      } else if (parsed.kind == SelectionOperationKind::Add) {
        operation.objects.push_back({semantic.target, sceneMatches.front()});
        if (std::find(resolvedSelection.begin(), resolvedSelection.end(),
                      sceneMatches.front()) == resolvedSelection.end())
          resolvedSelection.push_back(sceneMatches.front());
      } else {
        const auto selectedMatches = ResolveSelected(
            context.scene, command.target, id, resolvedSelection);
        for (const std::string &uuid : selectedMatches)
          operation.objects.push_back({semantic.target, uuid});
        for (const std::string &uuid : selectedMatches)
          resolvedSelection.erase(
              std::remove(resolvedSelection.begin(), resolvedSelection.end(),
                          uuid),
              resolvedSelection.end());
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
