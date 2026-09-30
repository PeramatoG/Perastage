#include "command_selection.h"

#include "mvrscene.h"

#include <algorithm>
#include <exception>

namespace perastage::command::selection {
namespace {

// Returns the stable machine token for an object kind.
const char *KindToken(ObjectKind kind) {
  switch (kind) {
  case ObjectKind::Fixture:
    return "fixture";
  case ObjectKind::Truss:
    return "truss";
  case ObjectKind::Support:
    return "support";
  case ObjectKind::SceneObject:
    return "scene_object";
  }
  return "invalid";
}

// Returns the selection bucket owned by an object kind.
std::vector<std::string> &Bucket(scene_grouping::ObjectSelection &selection,
                                 ObjectKind kind) {
  switch (kind) {
  case ObjectKind::Fixture:
    return selection.fixtures;
  case ObjectKind::Truss:
    return selection.trusses;
  case ObjectKind::Support:
    return selection.supports;
  case ObjectKind::SceneObject:
    return selection.sceneObjects;
  }
  return selection.fixtures;
}

// Reports whether a UUID identifies an object of the declared scene kind.
bool Exists(const MvrScene &scene, const ObjectReference &object) {
  switch (object.kind) {
  case ObjectKind::Fixture:
    return scene.fixtures.contains(object.uuid);
  case ObjectKind::Truss:
    return scene.trusses.contains(object.uuid);
  case ObjectKind::Support:
    return scene.supports.contains(object.uuid);
  case ObjectKind::SceneObject:
    return scene.sceneObjects.contains(object.uuid);
  }
  return false;
}

// Compares every ordered category in a selection.
bool Equal(const scene_grouping::ObjectSelection &left,
           const scene_grouping::ObjectSelection &right) {
  return left.fixtures == right.fixtures && left.trusses == right.trusses &&
         left.supports == right.supports &&
         left.sceneObjects == right.sceneObjects;
}

} // namespace

// Projects a typed selection update into the stable generic request contract.
Request BuildRequest(const Command &command) {
  Request request{kUpdateCommandId,
                  {{"target_kind", std::string(KindToken(command.target))},
                   {"preserve_existing", command.preserveExisting}}};
  std::vector<std::string> operationKinds;
  std::vector<std::string> objectKinds;
  std::vector<std::string> objectUuids;
  for (const Operation &operation : command.operations) {
    for (const ObjectReference &object : operation.objects) {
      operationKinds.emplace_back(operation.kind == OperationKind::Add
                                      ? "add"
                                      : "remove");
      objectKinds.emplace_back(KindToken(object.kind));
      objectUuids.push_back(object.uuid);
    }
  }
  request.arguments.push_back({"operation_kinds", operationKinds});
  request.arguments.push_back({"object_kinds", objectKinds});
  request.arguments.push_back({"object_uuids", objectUuids});
  return request;
}

// Applies UUID-based selection operations without mutating project content.
Result Execute(const Command &command, ExecutionContext &context) {
  Result result;
  result.request = BuildRequest(command);
  for (const Operation &operation : command.operations) {
    for (const ObjectReference &object : operation.objects) {
      if (object.kind != command.target || object.uuid.empty() ||
          !Exists(context.scene, object)) {
        result.outcome = Outcome::ValidationError;
        result.diagnostics.push_back(
            {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
             "scene.selection.invalid_object",
             "A selection object must identify an existing object of the target kind.",
             "object_uuids"});
        return result;
      }
    }
  }

  const scene_grouping::ObjectSelection before = context.selection;
  auto &selected = Bucket(context.selection, command.target);
  if (!command.preserveExisting)
    selected.clear();
  for (const Operation &operation : command.operations) {
    for (const ObjectReference &object : operation.objects) {
      const auto found = std::find(selected.begin(), selected.end(), object.uuid);
      if (operation.kind == OperationKind::Add) {
        if (found == selected.end())
          selected.push_back(object.uuid);
      } else {
        selected.erase(std::remove(selected.begin(), selected.end(), object.uuid),
                       selected.end());
      }
    }
  }
  result.mutation.selectionChanged = !Equal(before, context.selection);
  if (!result.mutation.selectionChanged)
    result.diagnostics.push_back(
        {DiagnosticSeverity::Information, DiagnosticPhase::Execution,
         "scene.selection.noop", "Selection is already in the requested state."});
  return result;
}

// Clears the historically supported Console selection categories transactionally.
Result ExecuteClear(ExecutionContext &context) {
  Result result;
  result.request = Request{kClearCommandId, {}};
  const scene_grouping::ObjectSelection before = context.selection;
  try {
    context.selection.fixtures.clear();
    context.selection.trusses.clear();
    context.selection.sceneObjects.clear();
    result.mutation.selectionChanged = !Equal(before, context.selection);
    const MutationPublication publication =
        context.mutationHost.CommitMutation(context.scene, before, "cli clear");
    result.mutation.undoEntryRecorded = publication.undoEntryRecorded;
    result.mutation.projectDirty = publication.projectDirty;
    if (!result.mutation.selectionChanged)
      result.diagnostics.push_back(
          {DiagnosticSeverity::Information, DiagnosticPhase::Execution,
           "scene.selection.clear_noop",
           "Selection was already clear; the legacy Undo entry was retained."});
  } catch (const std::exception &error) {
    context.selection = before;
    result.outcome = Outcome::ExecutionError;
    result.mutation = {};
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Execution,
         "scene.selection.clear_failed",
         std::string("Selection clear failed: ") + error.what()});
  } catch (...) {
    context.selection = before;
    result.outcome = Outcome::ExecutionError;
    result.mutation = {};
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Execution,
         "scene.selection.clear_failed", "Selection clear failed."});
  }
  return result;
}

} // namespace perastage::command::selection
