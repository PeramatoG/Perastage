#include "command/command_scene_tools.h"

#include "fixture.h"
#include "sceneobject.h"
#include "truss.h"

#include <cassert>
#include <stdexcept>

using namespace perastage::command;

namespace {

class RecordingHost final : public ProjectMutationHost {
public:
  // Records publication attempts and optionally simulates a host failure.
  MutationPublication
  CommitMutation(const MvrScene &,
                 const scene_grouping::ObjectSelection &before,
                 const std::string &) override {
    ++commits;
    capturedSelection = before;
    if (fail)
      throw std::runtime_error("publish failed");
    return {true, true};
  }

  int commits = 0;
  bool fail = false;
  scene_grouping::ObjectSelection capturedSelection;
};

// Adds the two differently typed objects used by grouping scenarios.
void AddGroupableObjects(MvrScene &scene) {
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.layer = "Layer";
  fixture.transform.o = {100.0f, 0.0f, 0.0f};
  scene.fixtures[fixture.uuid] = fixture;
  Truss truss;
  truss.uuid = "truss";
  truss.layer = "Layer";
  truss.transform.o = {300.0f, 0.0f, 0.0f};
  scene.trusses[truss.uuid] = truss;
}

} // namespace

// Verifies grouping and conversion commands, outputs, no-ops, and rollback.
int main() {
  MvrScene scene;
  AddGroupableObjects(scene);
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {"fixture"};
  selection.trusses = {"truss"};
  RecordingHost host;
  ExecutionContext context{scene, selection, host};

  scene_tools::GroupCommand group{selection};
  assert(scene_tools::BuildGroupRequest(group, false).commandId ==
         scene_tools::kGroupCreateCommandId);
  Result grouped = scene_tools::ExecuteGroup(group, context);
  assert(grouped.Success() && grouped.mutation.sceneChanged);
  assert(grouped.mutation.selectionChanged && grouped.outputs.size() == 5);
  assert(host.commits == 1 && scene.groupObjects.size() == 1);
  const std::string groupUuid =
      std::get<std::string>(grouped.outputs.front().value);
  assert(!groupUuid.empty());
  assert(scene.fixtures["fixture"].parentGroupUuid == groupUuid);
  assert(scene.fixtures["fixture"].transform.o[0] == 100.0f);

  Result ungrouped = scene_tools::ExecuteUngroup(group, context);
  assert(ungrouped.Success() && ungrouped.mutation.sceneChanged);
  assert(host.commits == 2 && scene.groupObjects.empty());
  Result noop = scene_tools::ExecuteUngroup(group, context);
  assert(noop.Success() && !noop.mutation.HasSemanticChanges());
  assert(host.commits == 2);

  scene_tools::GroupCommand missing{
      {.fixtures = {"missing"}, .trusses = {"truss"}}};
  assert(scene_tools::ExecuteGroup(missing, context).outcome ==
         Outcome::ValidationError);

  MvrScene rollbackScene;
  AddGroupableObjects(rollbackScene);
  scene_grouping::ObjectSelection rollbackSelection = group.objects;
  RecordingHost failingHost;
  failingHost.fail = true;
  ExecutionContext rollbackContext{rollbackScene, rollbackSelection,
                                   failingHost};
  Result rolledBack = scene_tools::ExecuteGroup(group, rollbackContext);
  assert(rolledBack.outcome == Outcome::ExecutionError);
  assert(rollbackScene.groupObjects.empty());
  assert(rollbackScene.fixtures["fixture"].parentGroupUuid.empty());
  assert(!rolledBack.mutation.HasSemanticChanges());

  MvrScene conversionScene;
  Fixture convertible;
  convertible.uuid = "convertible";
  conversionScene.fixtures[convertible.uuid] = convertible;
  scene_grouping::ObjectSelection conversionSelection;
  conversionSelection.fixtures = {convertible.uuid};
  RecordingHost conversionHost;
  ExecutionContext conversionContext{conversionScene, conversionSelection,
                                     conversionHost};
  Result fixtureConversion = scene_tools::ExecuteFixtureToSupport(
      {{convertible.uuid}}, conversionContext);
  assert(fixtureConversion.Success() && conversionHost.commits == 1);
  assert(!conversionScene.fixtures.contains(convertible.uuid));
  assert(conversionScene.supports.contains(convertible.uuid));
  assert(conversionSelection.fixtures.empty());
  assert(conversionSelection.supports ==
         std::vector<std::string>{convertible.uuid});

  MvrScene conversionRollbackScene;
  conversionRollbackScene.fixtures[convertible.uuid] = convertible;
  scene_grouping::ObjectSelection conversionRollbackSelection;
  conversionRollbackSelection.fixtures = {convertible.uuid};
  RecordingHost conversionFailingHost;
  conversionFailingHost.fail = true;
  ExecutionContext conversionRollbackContext{conversionRollbackScene,
                                             conversionRollbackSelection,
                                             conversionFailingHost};
  Result conversionRollback = scene_tools::ExecuteFixtureToSupport(
      {{convertible.uuid}}, conversionRollbackContext);
  assert(conversionRollback.outcome == Outcome::ExecutionError);
  assert(conversionRollbackScene.fixtures.contains(convertible.uuid));
  assert(!conversionRollbackScene.supports.contains(convertible.uuid));
  assert(!conversionRollback.mutation.HasSemanticChanges());

  SceneObject first;
  first.uuid = "object-a";
  first.modelFile = "truss.glb";
  SceneObject second = first;
  second.uuid = "object-b";
  conversionScene.sceneObjects[first.uuid] = first;
  conversionScene.sceneObjects[second.uuid] = second;
  conversionSelection.sceneObjects = {first.uuid};
  Result trussConversion = scene_tools::ExecuteSceneObjectsToTrusses(
      {first.uuid}, conversionContext);
  assert(trussConversion.Success() && conversionHost.commits == 2);
  assert(conversionScene.sceneObjects.empty());
  assert(conversionSelection.trusses ==
         (std::vector<std::string>{"object-a", "object-b"}));
  assert(trussConversion.outputs.size() == 2);
  assert(
      scene_tools::ExecuteSceneObjectsToTrusses({"missing"}, conversionContext)
          .outcome == Outcome::ValidationError);
  return 0;
}
