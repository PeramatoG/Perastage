#include "command_text_processor.h"

#include "command/command_selection.h"
#include "command/command_selection_text_adapter.h"
#include "command/command_transform_text_adapter.h"

#include <variant>

namespace perastage::command::text {
namespace {

// Merges one semantic mutation report into the line-level summary.
void MergeMutation(MutationSummary &summary, const MutationSummary &mutation) {
  summary.sceneChanged = summary.sceneChanged || mutation.sceneChanged;
  summary.selectionChanged =
      summary.selectionChanged || mutation.selectionChanged;
  summary.projectMetadataChanged =
      summary.projectMetadataChanged || mutation.projectMetadataChanged;
  summary.undoEntryRecorded =
      summary.undoEntryRecorded || mutation.undoEntryRecorded;
  summary.projectDirty = summary.projectDirty || mutation.projectDirty;
}

// Dispatches one parsed text command to its existing semantic command service.
Result ExecuteParsedCommand(
    const ParsedCommand &command, ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &transformPolicy) {
  if (std::holds_alternative<ClearCommand>(command))
    return selection::ExecuteClear(context);
  if (const auto *selectionCommand = std::get_if<SelectionCommand>(&command))
    return ExecuteSelection(*selectionCommand, context);
  return transform::Execute(AdaptTransform(std::get<TransformCommand>(command)),
                            context, transformPolicy);
}

} // namespace

// Reports whether parsing and every attempted semantic command succeeded.
bool LineExecutionResult::Success() const {
  return parseDiagnostics.empty() && !stopped;
}

// Parses and executes one Console command line in deterministic source order.
LineExecutionResult ProcessCommandLine(
    const std::string &line, ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &transformPolicy) {
  LineExecutionResult execution;
  ParseResult parsed = ParseCommandLine(line);
  execution.parseDiagnostics = std::move(parsed.diagnostics);
  for (const ParsedCommand &command : parsed.commands) {
    Result result = ExecuteParsedCommand(command, context, transformPolicy);
    MergeMutation(execution.mutation, result.mutation);
    const bool succeeded = result.Success();
    execution.records.push_back(
        {command, std::move(result), context.selection});
    if (!succeeded) {
      execution.stopped = true;
      break;
    }
  }
  if (!execution.parseDiagnostics.empty())
    execution.stopped = true;
  return execution;
}

} // namespace perastage::command::text
