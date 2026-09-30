#include "command_scene_tools.h"

#include "command/command_mutation_transaction.h"
#include "scene_node_operations.h"
#include "scene_object_truss_converter.h"

#include <algorithm>
#include <exception>
#include <set>

namespace perastage::command::scene_tools {
namespace {

// Counts all explicit object identities in a grouping request.
std::size_t ObjectCount(const scene_grouping::ObjectSelection &objects) {
  return objects.fixtures.size() + objects.trusses.size() +
         objects.supports.size() + objects.sceneObjects.size();
}

// Reports whether every requested identity exists in its declared container.
bool ValidObjects(const MvrScene &scene,
                  const scene_grouping::ObjectSelection &objects) {
  const auto allExist = [](const auto &container, const auto &uuids) {
    return std::all_of(uuids.begin(), uuids.end(),
                       [&](const std::string &uuid) {
                         return !uuid.empty() && container.contains(uuid);
                       });
  };
  return allExist(scene.fixtures, objects.fixtures) &&
         allExist(scene.trusses, objects.trusses) &&
         allExist(scene.supports, objects.supports) &&
         allExist(scene.sceneObjects, objects.sceneObjects);
}

// Replaces semantic selection with the affected object identities.
void SelectAffected(scene_grouping::ObjectSelection &selection,
                    const scene_grouping::OperationResult &operation) {
  selection = {.fixtures = operation.affectedFixtures,
               .trusses = operation.affectedTrusses,
               .supports = operation.affectedSupports,
               .sceneObjects = operation.affectedSceneObjects};
}

// Appends stable grouping arguments in category order.
void AppendObjectArguments(Request &request,
                           const scene_grouping::ObjectSelection &objects) {
  request.arguments.push_back({"fixture_uuids", objects.fixtures});
  request.arguments.push_back({"truss_uuids", objects.trusses});
  request.arguments.push_back({"support_uuids", objects.supports});
  request.arguments.push_back({"scene_object_uuids", objects.sceneObjects});
}

// Returns a deterministic execution failure and relies on transaction rollback.
Result ExecutionFailure(Result result, const std::string &code,
                        const std::string &message) {
  result.outcome = Outcome::ExecutionError;
  result.mutation = {};
  result.outputs.clear();
  result.diagnostics.push_back(
      {DiagnosticSeverity::Error, DiagnosticPhase::Execution, code, message});
  return result;
}

} // namespace

// Projects a grouping command onto the stable generic request contract.
Request BuildGroupRequest(const GroupCommand &command, bool ungroup) {
  Request request{ungroup ? kGroupUngroupCommandId : kGroupCreateCommandId, {}};
  AppendObjectArguments(request, command.objects);
  return request;
}

// Projects fixture conversion targets onto the stable request contract.
Request BuildFixtureToSupportRequest(const FixtureToSupportCommand &command) {
  return {kFixtureToSupportCommandId,
          {{"fixture_uuids", command.fixtureUuids}}};
}

// Projects same-model truss conversion onto the stable request contract.
Request
BuildSceneObjectsToTrussesRequest(const SceneObjectsToTrussesCommand &command) {
  return {kSceneObjectsToTrussesCommandId,
          {{"source_scene_object_uuid", command.sourceSceneObjectUuid}}};
}

// Creates one group and publishes its scene and selection mutation atomically.
Result ExecuteGroup(const GroupCommand &command, ExecutionContext &context) {
  Result result;
  result.request = BuildGroupRequest(command, false);
  if (ObjectCount(command.objects) < 2 ||
      !ValidObjects(context.scene, command.objects)) {
    result.outcome = Outcome::ValidationError;
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.group.invalid_objects",
         "Grouping requires at least two existing explicitly typed objects.",
         "fixture_uuids"});
    return result;
  }
  MutationTransaction transaction(context);
  try {
    const auto operation =
        scene_grouping::GroupSelection(context.scene, command.objects);
    if (!operation.changed) {
      result.diagnostics.push_back(
          {DiagnosticSeverity::Information, DiagnosticPhase::Execution,
           "scene.group.noop",
           "The requested objects cannot form a new group."});
      return result;
    }
    SelectAffected(context.selection, operation);
    result.outputs.push_back({"group_uuid", operation.groupUuid});
    result.outputs.push_back(
        {"affected_fixture_uuids", operation.affectedFixtures});
    result.outputs.push_back(
        {"affected_truss_uuids", operation.affectedTrusses});
    result.outputs.push_back(
        {"affected_support_uuids", operation.affectedSupports});
    result.outputs.push_back(
        {"affected_scene_object_uuids", operation.affectedSceneObjects});
    result.mutation =
        transaction.Commit({true, true}, "group selected elements");
  } catch (const std::exception &error) {
    return ExecutionFailure(std::move(result), "scene.group.failed",
                            std::string("Grouping failed: ") + error.what());
  } catch (...) {
    return ExecutionFailure(std::move(result), "scene.group.failed",
                            "Grouping failed.");
  }
  return result;
}

// Ungroups effective selected groups and publishes the mutation atomically.
Result ExecuteUngroup(const GroupCommand &command, ExecutionContext &context) {
  Result result;
  result.request = BuildGroupRequest(command, true);
  if (ObjectCount(command.objects) == 0 ||
      !ValidObjects(context.scene, command.objects)) {
    result.outcome = Outcome::ValidationError;
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.group.invalid_objects",
         "Ungrouping requires existing explicitly typed objects."});
    return result;
  }
  MutationTransaction transaction(context);
  try {
    const auto operation =
        scene_grouping::UngroupSelection(context.scene, command.objects);
    if (!operation.changed) {
      result.diagnostics.push_back({DiagnosticSeverity::Information,
                                    DiagnosticPhase::Execution,
                                    "scene.group.ungroup_noop",
                                    "The requested objects are not grouped."});
      return result;
    }
    SelectAffected(context.selection, operation);
    result.outputs.push_back(
        {"affected_fixture_uuids", operation.affectedFixtures});
    result.outputs.push_back(
        {"affected_truss_uuids", operation.affectedTrusses});
    result.outputs.push_back(
        {"affected_support_uuids", operation.affectedSupports});
    result.outputs.push_back(
        {"affected_scene_object_uuids", operation.affectedSceneObjects});
    result.mutation =
        transaction.Commit({true, true}, "ungroup selected elements");
  } catch (const std::exception &error) {
    return ExecutionFailure(std::move(result), "scene.group.ungroup_failed",
                            std::string("Ungrouping failed: ") + error.what());
  } catch (...) {
    return ExecutionFailure(std::move(result), "scene.group.ungroup_failed",
                            "Ungrouping failed.");
  }
  return result;
}

// Converts explicit fixture UUIDs to supports as one atomic command.
Result ExecuteFixtureToSupport(const FixtureToSupportCommand &command,
                               ExecutionContext &context) {
  Result result;
  result.request = BuildFixtureToSupportRequest(command);
  std::set<std::string> unique;
  for (const auto &uuid : command.fixtureUuids) {
    if (uuid.empty() || !unique.insert(uuid).second ||
        !context.scene.fixtures.contains(uuid) ||
        context.scene.supports.contains(uuid)) {
      result.outcome = Outcome::ValidationError;
      result.diagnostics.push_back(
          {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
           "scene.convert.invalid_fixture",
           "Every fixture UUID must be unique, existing, and convertible.",
           "fixture_uuids"});
      return result;
    }
  }
  if (command.fixtureUuids.empty()) {
    result.outcome = Outcome::ValidationError;
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.convert.empty_fixture_scope",
         "At least one fixture UUID is required.", "fixture_uuids"});
    return result;
  }
  MutationTransaction transaction(context);
  try {
    std::vector<std::string> cleared;
    for (const auto &uuid : command.fixtureUuids) {
      const auto conversion =
          scene_node_operations::ConvertFixtureToSupport(context.scene, uuid);
      if (!conversion.changed)
        return ExecutionFailure(std::move(result),
                                "scene.convert.fixture_failed",
                                "Fixture conversion failed.");
      cleared.insert(cleared.end(),
                     conversion.clearedMotorFixtureReferences.begin(),
                     conversion.clearedMotorFixtureReferences.end());
    }
    context.selection.fixtures.clear();
    context.selection.supports = command.fixtureUuids;
    result.outputs.push_back({"converted_uuids", command.fixtureUuids});
    result.outputs.push_back({"cleared_motor_reference_uuids", cleared});
    result.mutation =
        transaction.Commit({true, true}, "convert fixtures to hoists");
  } catch (const std::exception &error) {
    return ExecutionFailure(std::move(result), "scene.convert.fixture_failed",
                            std::string("Fixture conversion failed: ") +
                                error.what());
  } catch (...) {
    return ExecutionFailure(std::move(result), "scene.convert.fixture_failed",
                            "Fixture conversion failed.");
  }
  return result;
}

// Converts the explicit source object's same-model scope to trusses atomically.
Result ExecuteSceneObjectsToTrusses(const SceneObjectsToTrussesCommand &command,
                                    ExecutionContext &context) {
  Result result;
  result.request = BuildSceneObjectsToTrussesRequest(command);
  if (command.sourceSceneObjectUuid.empty() ||
      !context.scene.sceneObjects.contains(command.sourceSceneObjectUuid)) {
    result.outcome = Outcome::ValidationError;
    result.diagnostics.push_back(
        {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
         "scene.convert.invalid_scene_object",
         "The source UUID must identify an existing scene object.",
         "source_scene_object_uuid"});
    return result;
  }
  MutationTransaction transaction(context);
  try {
    const auto conversion = ConvertSceneObjectsWithSameModelToTrusses(
        context.scene, command.sourceSceneObjectUuid);
    if (conversion.convertedUuids.empty()) {
      result.outcome = Outcome::ValidationError;
      result.diagnostics.push_back(
          {DiagnosticSeverity::Error, DiagnosticPhase::Validation,
           "scene.convert.missing_model",
           "The source scene object has no convertible model.",
           "source_scene_object_uuid"});
      return result;
    }
    std::vector<std::string> convertedUuids = conversion.convertedUuids;
    std::sort(convertedUuids.begin(), convertedUuids.end());
    context.selection.sceneObjects.clear();
    context.selection.trusses = convertedUuids;
    result.outputs.push_back({"converted_uuids", convertedUuids});
    result.outputs.push_back({"model_file", conversion.modelFile});
    result.mutation =
        transaction.Commit({true, true}, "convert scene objects to trusses");
  } catch (const std::exception &error) {
    return ExecutionFailure(std::move(result), "scene.convert.truss_failed",
                            std::string("Scene-object conversion failed: ") +
                                error.what());
  } catch (...) {
    return ExecutionFailure(std::move(result), "scene.convert.truss_failed",
                            "Scene-object conversion failed.");
  }
  return result;
}

} // namespace perastage::command::scene_tools
