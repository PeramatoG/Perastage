#pragma once

#include "command/command_contract.h"
#include "transform_space.h"

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace perastage::command::text {

enum class SelectionTarget { Fixtures, Trusses };
enum class SelectionOperationKind { Add, Remove };
enum class TransformKind { Position, Rotation };

struct SelectionOperation {
  SelectionOperationKind kind = SelectionOperationKind::Add;
  int firstId = 0;
  int lastId = 0;
};

struct ClearCommand {};

struct SelectionCommand {
  SelectionTarget target = SelectionTarget::Fixtures;
  std::vector<SelectionOperation> operations;
};

struct TransformComponent {
  int axis = 0;
  std::vector<float> values;
  bool relative = false;
  bool group = false;
  transform_space::TransformSpace space =
      transform_space::TransformSpace::World;
};

struct TransformCommand {
  TransformKind kind = TransformKind::Position;
  std::vector<TransformComponent> components;
  std::optional<std::array<float, 3>> pivotMm;
};

using ParsedCommand =
    std::variant<ClearCommand, SelectionCommand, TransformCommand>;

struct ParseResult {
  std::vector<ParsedCommand> commands;
  std::vector<Diagnostic> diagnostics;

  bool Success() const;
};

struct TransformCommandSegment {
  std::vector<float> values;
  bool relative = false;
  bool group = false;
  transform_space::TransformSpace space =
      transform_space::TransformSpace::World;
  std::string remainder;
};

// Parses transform values and modifiers from one command segment.
TransformCommandSegment ParseTransformCommandSegment(const std::string &text);

// Parses transform values while retaining the legacy relative flag API.
std::vector<float> ParseTransformValues(const std::string &text,
                                        bool &relative);

// Parses one complete Console input line into ordered neutral syntax.
ParseResult ParseCommandLine(const std::string &text);

} // namespace perastage::command::text
