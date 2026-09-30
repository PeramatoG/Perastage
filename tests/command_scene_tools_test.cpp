#include "command/command_scene_tools.h"

#include "fixture.h"
#include "groupobject.h"
#include "matrixutils.h"
#include "scene_object_truss_converter.h"
#include "sceneobject.h"
#include "support.h"
#include "truss.h"

#include <algorithm>
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

// Compares all ordered semantic selection buckets.
bool EqualSelection(const scene_grouping::ObjectSelection &left,
                    const scene_grouping::ObjectSelection &right) {
  return left.fixtures == right.fixtures && left.trusses == right.trusses &&
         left.supports == right.supports &&
         left.sceneObjects == right.sceneObjects;
}

// Compares complete transform matrices exactly for rollback assertions.
bool EqualMatrix(const Matrix &left, const Matrix &right) {
  return left.u == right.u && left.v == right.v && left.w == right.w &&
         left.o == right.o;
}

// Reports whether a result contains a diagnostic with the requested code.
bool HasDiagnostic(const Result &result, const std::string &code) {
  return std::any_of(
      result.diagnostics.begin(), result.diagnostics.end(),
      [&](const Diagnostic &diagnostic) { return diagnostic.code == code; });
}

// Returns a string-vector command output by its stable identifier.
std::vector<std::string> StringVectorOutput(const Result &result,
                                            const std::string &id) {
  const auto found =
      std::find_if(result.outputs.begin(), result.outputs.end(),
                   [&](const Argument &output) { return output.id == id; });
  assert(found != result.outputs.end());
  return std::get<std::vector<std::string>>(found->value);
}

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

// Builds a grouped fixture and a support that references it as a motor fixture.
MvrScene BuildFixtureConversionScene() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture-a";
  fixture.parentGroupUuid = "fixture-group";
  fixture.transform = MatrixUtils::Identity();
  fixture.transform.o = {120.0f, 230.0f, 340.0f};
  fixture.localTransform = MatrixUtils::Identity();
  fixture.localTransform.o = {20.0f, 30.0f, 40.0f};
  fixture.hasLocalTransform = true;
  scene.fixtures[fixture.uuid] = fixture;

  Support linkedSupport;
  linkedSupport.uuid = "support-b";
  linkedSupport.motorFixtureUuid = fixture.uuid;
  scene.supports[linkedSupport.uuid] = linkedSupport;

  GroupObject group;
  group.uuid = fixture.parentGroupUuid;
  group.children.push_back({MvrNodeType::Fixture, fixture.uuid});
  scene.groupObjects[group.uuid] = group;
  return scene;
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
  assert(!grouped.mutation.selectionChanged && grouped.outputs.size() == 5);
  assert(host.commits == 1 && scene.groupObjects.size() == 1);
  const std::string groupUuid =
      std::get<std::string>(grouped.outputs.front().value);
  assert(!groupUuid.empty());
  assert(scene.fixtures["fixture"].parentGroupUuid == groupUuid);
  assert(scene.fixtures["fixture"].transform.o[0] == 100.0f);

  selection = {.fixtures = {"fixture"}};
  Result ungrouped = scene_tools::ExecuteUngroup(group, context);
  assert(ungrouped.Success() && ungrouped.mutation.sceneChanged);
  assert(ungrouped.mutation.selectionChanged);
  assert(selection.fixtures == std::vector<std::string>{"fixture"});
  assert(selection.trusses == std::vector<std::string>{"truss"});
  assert(host.commits == 2 && scene.groupObjects.empty());
  Result noop = scene_tools::ExecuteUngroup(group, context);
  assert(noop.Success() && !noop.mutation.HasSemanticChanges());
  assert(host.commits == 2);

  const scene_grouping::ObjectSelection duplicateSelectionBefore = selection;
  const std::size_t groupCountBeforeDuplicates = scene.groupObjects.size();
  for (const scene_tools::GroupCommand duplicate :
       {scene_tools::GroupCommand{{.fixtures = {"fixture", "fixture"}}},
        scene_tools::GroupCommand{{.trusses = {"truss", "truss"}}}}) {
    const Result duplicateResult =
        scene_tools::ExecuteGroup(duplicate, context);
    assert(duplicateResult.outcome == Outcome::ValidationError);
    assert(HasDiagnostic(duplicateResult, "scene.group.duplicate_object"));
    assert(duplicateResult.outputs.empty());
    assert(!duplicateResult.mutation.HasSemanticChanges());
    assert(!duplicateResult.mutation.undoEntryRecorded);
    assert(!duplicateResult.mutation.projectDirty);
    assert(host.commits == 2);
    assert(scene.groupObjects.size() == groupCountBeforeDuplicates);
    assert(EqualSelection(selection, duplicateSelectionBefore));
  }

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
  assert(rolledBack.outputs.empty());
  assert(!rolledBack.mutation.HasSemanticChanges());
  assert(!rolledBack.mutation.undoEntryRecorded);
  assert(!rolledBack.mutation.projectDirty);

  MvrScene ungroupRollbackScene;
  AddGroupableObjects(ungroupRollbackScene);
  scene_grouping::ObjectSelection ungroupRollbackSelection = group.objects;
  const auto ungroupSetup = scene_grouping::GroupSelection(
      ungroupRollbackScene, ungroupRollbackSelection);
  assert(ungroupSetup.changed);
  const std::string rollbackGroupUuid = ungroupSetup.groupUuid;
  const Matrix fixtureWorldBeforeUngroup =
      ungroupRollbackScene.fixtures["fixture"].transform;
  const Matrix fixtureLocalBeforeUngroup =
      ungroupRollbackScene.fixtures["fixture"].localTransform;
  const Matrix trussWorldBeforeUngroup =
      ungroupRollbackScene.trusses["truss"].transform;
  const Matrix trussLocalBeforeUngroup =
      ungroupRollbackScene.trusses["truss"].localTransform;
  const scene_grouping::ObjectSelection selectionBeforeUngroup =
      ungroupRollbackSelection;
  RecordingHost ungroupFailingHost;
  ungroupFailingHost.fail = true;
  ExecutionContext ungroupRollbackContext{
      ungroupRollbackScene, ungroupRollbackSelection, ungroupFailingHost};
  const Result ungroupRollback =
      scene_tools::ExecuteUngroup(group, ungroupRollbackContext);
  assert(ungroupRollback.outcome == Outcome::ExecutionError);
  assert(ungroupRollback.request.has_value());
  assert(ungroupRollback.request->commandId ==
         scene_tools::kGroupUngroupCommandId);
  assert(HasDiagnostic(ungroupRollback, "scene.group.ungroup_failed"));
  assert(ungroupRollback.outputs.empty());
  assert(!ungroupRollback.mutation.HasSemanticChanges());
  assert(!ungroupRollback.mutation.undoEntryRecorded);
  assert(!ungroupRollback.mutation.projectDirty);
  assert(ungroupRollbackScene.groupObjects.contains(rollbackGroupUuid));
  assert(ungroupRollbackScene.groupObjects[rollbackGroupUuid].children.size() ==
         2);
  assert(
      ungroupRollbackScene.groupObjects[rollbackGroupUuid].children[0].uuid ==
      "fixture");
  assert(
      ungroupRollbackScene.groupObjects[rollbackGroupUuid].children[1].uuid ==
      "truss");
  assert(ungroupRollbackScene.fixtures["fixture"].parentGroupUuid ==
         rollbackGroupUuid);
  assert(ungroupRollbackScene.trusses["truss"].parentGroupUuid ==
         rollbackGroupUuid);
  assert(EqualMatrix(ungroupRollbackScene.fixtures["fixture"].transform,
                     fixtureWorldBeforeUngroup));
  assert(EqualMatrix(ungroupRollbackScene.fixtures["fixture"].localTransform,
                     fixtureLocalBeforeUngroup));
  assert(EqualMatrix(ungroupRollbackScene.trusses["truss"].transform,
                     trussWorldBeforeUngroup));
  assert(EqualMatrix(ungroupRollbackScene.trusses["truss"].localTransform,
                     trussLocalBeforeUngroup));
  assert(EqualSelection(ungroupRollbackSelection, selectionBeforeUngroup));

  MvrScene conversionScene = BuildFixtureConversionScene();
  const Fixture convertible = conversionScene.fixtures.at("fixture-a");
  const Matrix fixtureWorldBeforeConversion = convertible.transform;
  const Matrix fixtureLocalBeforeConversion = convertible.localTransform;
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
  assert(conversionScene.supports["support-b"].motorFixtureUuid.empty());
  assert(
      StringVectorOutput(fixtureConversion, "cleared_motor_reference_uuids") ==
      std::vector<std::string>{"support-b"});
  assert(conversionScene.supports[convertible.uuid].parentGroupUuid ==
         "fixture-group");
  assert(conversionScene.groupObjects["fixture-group"].children.size() == 1);
  assert(conversionScene.groupObjects["fixture-group"].children.front().type ==
         MvrNodeType::Support);
  assert(EqualMatrix(conversionScene.supports[convertible.uuid].transform,
                     fixtureWorldBeforeConversion));
  assert(EqualMatrix(conversionScene.supports[convertible.uuid].localTransform,
                     fixtureLocalBeforeConversion));
  assert(conversionScene.supports[convertible.uuid].hasLocalTransform);
  assert(conversionSelection.fixtures.empty());
  assert(conversionSelection.supports ==
         std::vector<std::string>{convertible.uuid});
  assert(fixtureConversion.mutation.selectionChanged);

  MvrScene unchangedSelectionScene = BuildFixtureConversionScene();
  scene_grouping::ObjectSelection unchangedConversionSelection;
  unchangedConversionSelection.supports = {convertible.uuid};
  RecordingHost unchangedSelectionHost;
  ExecutionContext unchangedSelectionContext{unchangedSelectionScene,
                                             unchangedConversionSelection,
                                             unchangedSelectionHost};
  Result unchangedSelectionConversion = scene_tools::ExecuteFixtureToSupport(
      {{convertible.uuid}}, unchangedSelectionContext);
  assert(unchangedSelectionConversion.mutation.sceneChanged);
  assert(!unchangedSelectionConversion.mutation.selectionChanged);

  MvrScene conversionRollbackScene = BuildFixtureConversionScene();
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
  assert(conversionRollbackScene.supports["support-b"].motorFixtureUuid ==
         convertible.uuid);
  assert(conversionRollbackScene.groupObjects["fixture-group"]
             .children.front()
             .type == MvrNodeType::Fixture);
  assert(conversionRollbackScene.fixtures[convertible.uuid].parentGroupUuid ==
         "fixture-group");
  assert(
      EqualMatrix(conversionRollbackScene.fixtures[convertible.uuid].transform,
                  fixtureWorldBeforeConversion));
  assert(EqualMatrix(
      conversionRollbackScene.fixtures[convertible.uuid].localTransform,
      fixtureLocalBeforeConversion));
  assert(conversionRollbackScene.fixtures[convertible.uuid].hasLocalTransform);
  assert(EqualSelection(
      conversionRollbackSelection,
      scene_grouping::ObjectSelection{.fixtures = {convertible.uuid}}));
  assert(conversionRollback.outputs.empty());
  assert(!conversionRollback.mutation.HasSemanticChanges());
  assert(!conversionRollback.mutation.undoEntryRecorded);
  assert(!conversionRollback.mutation.projectDirty);

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
  assert(trussConversion.mutation.selectionChanged);
  assert(conversionScene.sceneObjects.empty());
  assert(conversionSelection.trusses ==
         (std::vector<std::string>{"object-a", "object-b"}));
  assert(trussConversion.outputs.size() == 2);
  assert(
      scene_tools::ExecuteSceneObjectsToTrusses({"missing"}, conversionContext)
          .outcome == Outcome::ValidationError);

  MvrScene unchangedTrussSelectionScene;
  unchangedTrussSelectionScene.sceneObjects[first.uuid] = first;
  unchangedTrussSelectionScene.sceneObjects[second.uuid] = second;
  scene_grouping::ObjectSelection unchangedTrussSelection;
  unchangedTrussSelection.trusses = {"object-a", "object-b"};
  RecordingHost unchangedTrussSelectionHost;
  ExecutionContext unchangedTrussSelectionContext{unchangedTrussSelectionScene,
                                                  unchangedTrussSelection,
                                                  unchangedTrussSelectionHost};
  Result unchangedTrussSelectionConversion =
      scene_tools::ExecuteSceneObjectsToTrusses({first.uuid},
                                                unchangedTrussSelectionContext);
  assert(unchangedTrussSelectionConversion.mutation.sceneChanged);
  assert(!unchangedTrussSelectionConversion.mutation.selectionChanged);

  MvrScene trussRollbackScene;
  SceneObject groupedObject = first;
  groupedObject.uuid = "grouped-object";
  groupedObject.parentGroupUuid = "object-group";
  groupedObject.transform = MatrixUtils::Identity();
  groupedObject.transform.o = {410.0f, 520.0f, 630.0f};
  groupedObject.localTransform = MatrixUtils::Identity();
  groupedObject.localTransform.o = {10.0f, 20.0f, 30.0f};
  groupedObject.hasLocalTransform = true;
  SceneObject groupedPeer = groupedObject;
  groupedPeer.uuid = "grouped-peer";
  groupedPeer.parentGroupUuid.clear();
  trussRollbackScene.sceneObjects[groupedObject.uuid] = groupedObject;
  trussRollbackScene.sceneObjects[groupedPeer.uuid] = groupedPeer;
  GroupObject objectGroup;
  objectGroup.uuid = groupedObject.parentGroupUuid;
  objectGroup.children.push_back(
      {MvrNodeType::SceneObject, groupedObject.uuid});
  trussRollbackScene.groupObjects[objectGroup.uuid] = objectGroup;
  scene_grouping::ObjectSelection trussRollbackSelection;
  trussRollbackSelection.sceneObjects = {groupedObject.uuid};
  const scene_grouping::ObjectSelection trussSelectionBefore =
      trussRollbackSelection;
  RecordingHost trussFailingHost;
  trussFailingHost.fail = true;
  ExecutionContext trussRollbackContext{
      trussRollbackScene, trussRollbackSelection, trussFailingHost};
  const Result trussRollback = scene_tools::ExecuteSceneObjectsToTrusses(
      {groupedObject.uuid}, trussRollbackContext);
  assert(trussRollback.outcome == Outcome::ExecutionError);
  assert(trussRollback.request.has_value());
  assert(trussRollback.request->commandId ==
         scene_tools::kSceneObjectsToTrussesCommandId);
  assert(HasDiagnostic(trussRollback, "scene.convert.truss_failed"));
  assert(trussRollback.outputs.empty());
  assert(!trussRollback.mutation.HasSemanticChanges());
  assert(!trussRollback.mutation.undoEntryRecorded);
  assert(!trussRollback.mutation.projectDirty);
  assert(trussRollbackScene.sceneObjects.contains(groupedObject.uuid));
  assert(trussRollbackScene.sceneObjects.contains(groupedPeer.uuid));
  assert(trussRollbackScene.trusses.empty());
  assert(trussRollbackScene.groupObjects[objectGroup.uuid].children.size() ==
         1);
  assert(
      trussRollbackScene.groupObjects[objectGroup.uuid].children.front().type ==
      MvrNodeType::SceneObject);
  assert(trussRollbackScene.sceneObjects[groupedObject.uuid].parentGroupUuid ==
         objectGroup.uuid);
  assert(
      EqualMatrix(trussRollbackScene.sceneObjects[groupedObject.uuid].transform,
                  groupedObject.transform));
  assert(EqualMatrix(
      trussRollbackScene.sceneObjects[groupedObject.uuid].localTransform,
      groupedObject.localTransform));
  assert(trussRollbackScene.sceneObjects[groupedObject.uuid].hasLocalTransform);
  assert(EqualSelection(trussRollbackSelection, trussSelectionBefore));

  MvrScene collisionScene;
  SceneObject collisionSource = first;
  collisionSource.uuid = "object-b";
  SceneObject collisionPeer = first;
  collisionPeer.uuid = "object-c";
  SceneObject secondCollisionPeer = first;
  secondCollisionPeer.uuid = "object-a";
  collisionScene.sceneObjects[collisionSource.uuid] = collisionSource;
  collisionScene.sceneObjects[collisionPeer.uuid] = collisionPeer;
  collisionScene.sceneObjects[secondCollisionPeer.uuid] = secondCollisionPeer;
  Truss existingTrussA;
  existingTrussA.uuid = secondCollisionPeer.uuid;
  existingTrussA.name = "keep-a";
  collisionScene.trusses[existingTrussA.uuid] = existingTrussA;
  Truss existingTrussC;
  existingTrussC.uuid = collisionPeer.uuid;
  existingTrussC.name = "keep-c";
  collisionScene.trusses[existingTrussC.uuid] = existingTrussC;
  MvrScene directCollisionScene = collisionScene;
  const auto directCollision = ConvertSceneObjectsWithSameModelToTrusses(
      directCollisionScene, collisionSource.uuid);
  assert(directCollision.convertedUuids.empty());
  assert(directCollisionScene.sceneObjects.size() == 3);
  assert(directCollisionScene.trusses[secondCollisionPeer.uuid].name ==
         "keep-a");
  assert(directCollisionScene.trusses[collisionPeer.uuid].name == "keep-c");
  scene_grouping::ObjectSelection collisionSelection;
  collisionSelection.sceneObjects = {collisionSource.uuid};
  const scene_grouping::ObjectSelection collisionSelectionBefore =
      collisionSelection;
  RecordingHost collisionHost;
  ExecutionContext collisionContext{collisionScene, collisionSelection,
                                    collisionHost};
  Result collision = scene_tools::ExecuteSceneObjectsToTrusses(
      {collisionSource.uuid}, collisionContext);
  assert(collision.outcome == Outcome::ValidationError);
  assert(collision.diagnostics.size() == 1);
  assert(collision.diagnostics.front().code ==
         "scene.convert.truss_uuid_conflict");
  assert(collision.diagnostics.front().message ==
         "Scene-object conversion conflicts with existing truss UUID(s): "
         "object-a, object-c.");
  assert(!collision.mutation.HasSemanticChanges() && collision.outputs.empty());
  assert(!collision.mutation.undoEntryRecorded);
  assert(!collision.mutation.projectDirty);
  assert(collisionHost.commits == 0);
  assert(collisionScene.sceneObjects.size() == 3);
  assert(collisionScene.trusses[secondCollisionPeer.uuid].name == "keep-a");
  assert(collisionScene.trusses[collisionPeer.uuid].name == "keep-c");
  assert(collisionSelection.sceneObjects ==
         collisionSelectionBefore.sceneObjects);
  return 0;
}
