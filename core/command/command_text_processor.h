#pragma once

#include "command/command_execution.h"
#include "command/command_text_parser.h"

#include <string>
#include <vector>

namespace perastage::command::text {

struct ExecutionRecord {
  ParsedCommand command;
  Result result;
  scene_grouping::ObjectSelection selectionAfter;
};

struct LineExecutionResult {
  std::vector<ExecutionRecord> records;
  std::vector<Diagnostic> parseDiagnostics;
  MutationSummary mutation;
  bool stopped = false;

  // Reports whether parsing and every attempted semantic command succeeded.
  bool Success() const;
};

// Parses and executes one Console command line in deterministic source order.
LineExecutionResult ProcessCommandLine(
    const std::string &line, ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &transformPolicy);

} // namespace perastage::command::text
