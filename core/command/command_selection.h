#pragma once

#include "command/command_execution.h"

#include <string>
#include <vector>

namespace perastage::command::selection {

inline constexpr const char *kUpdateCommandId = "scene.selection.update";
inline constexpr const char *kClearCommandId = "scene.selection.clear";

enum class ObjectKind { Fixture, Truss, Support, SceneObject };
enum class OperationKind { Add, Remove };

struct ObjectReference {
  ObjectKind kind = ObjectKind::Fixture;
  std::string uuid;
};

struct Operation {
  OperationKind kind = OperationKind::Add;
  std::vector<ObjectReference> objects;
};

struct Command {
  ObjectKind target = ObjectKind::Fixture;
  bool preserveExisting = true;
  std::vector<Operation> operations;
};

// Projects a typed selection update into the stable generic request contract.
Request BuildRequest(const Command &command);

// Applies UUID-based selection operations without mutating project content.
Result Execute(const Command &command, ExecutionContext &context);

// Clears the historically supported Console selection categories
// transactionally.
Result ExecuteClear(ExecutionContext &context);

} // namespace perastage::command::selection
