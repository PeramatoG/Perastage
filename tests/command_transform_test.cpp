#include "command/command_transform.h"
#include "command/command_transform_text_adapter.h"
#include "matrixutils.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace perastage::command;

namespace {

class RecordingHost final : public ProjectMutationHost {
public:
  // Records publication details or simulates a host publication failure.
  MutationPublication
  CommitMutation(const MvrScene &sceneBefore,
                 const scene_grouping::ObjectSelection &selectionBefore,
                 const std::string &undoLabel) override {
    before = sceneBefore;
    selection = selectionBefore;
    label = undoLabel;
    ++publications;
    if (fail)
      throw std::runtime_error("publication failed");
    return {true, true};
  }

  MvrScene before;
  scene_grouping::ObjectSelection selection;
  std::string label;
  int publications = 0;
  bool fail = false;
};

// Builds an identity transform at the supplied world origin.
Matrix At(float x, float y = 0.0f, float z = 0.0f) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o = {x, y, z};
  return matrix;
}

// Compares floating-point transform values at test precision.
bool Near(float left, float right) { return std::fabs(left - right) < 0.01f; }

// Creates a semantic component with one value.
transform::Component Component(int axis, double value, bool relative = false,
                               transform_space::TransformSpace space =
                                   transform_space::TransformSpace::World) {
  return {axis, {value}, relative, false, space};
}

} // namespace

// Characterizes semantic transforms, transactions, diagnostics, and adaptation.
int main() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.transform = At(10.0f, 20.0f, 30.0f);
  scene.fixtures[fixture.uuid] = fixture;
  Truss truss;
  truss.uuid = "truss";
  truss.transform = At(100.0f);
  scene.trusses[truss.uuid] = truss;
  Support support;
  support.uuid = "support";
  support.transform = At(200.0f);
  scene.supports[support.uuid] = support;
  SceneObject object;
  object.uuid = "object";
  object.transform = At(300.0f);
  scene.sceneObjects[object.uuid] = object;

  scene_grouping::ObjectSelection selection;
  selection.fixtures = {fixture.uuid};
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  scene_grouping::InteractiveTransformPolicy policy;

  transform::Command absolute;
  absolute.components = {Component(0, 1000.0), Component(1, 2000.0),
                         Component(2, 3000.0)};
  Result result = transform::Execute(absolute, context, policy);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(result.mutation.undoEntryRecorded && result.mutation.projectDirty);
  assert(host.publications == 1 && host.label == "cli pos");
  assert(Near(scene.fixtures[fixture.uuid].transform.o[0], 1000.0f));
  assert(Near(scene.fixtures[fixture.uuid].transform.o[1], 2000.0f));
  assert(Near(scene.fixtures[fixture.uuid].transform.o[2], 3000.0f));
  assert(result.request->commandId == transform::kPositionCommandId);
  assert(result.request->arguments.front().id == "x_millimeters");

  transform::Command relative;
  relative.components = {Component(0, 50.0, true)};
  result = transform::Execute(relative, context, policy);
  assert(Near(scene.fixtures[fixture.uuid].transform.o[0], 1050.0f));
  assert(host.publications == 2);

  scene.fixtures[fixture.uuid].transform =
      MatrixUtils::EulerToMatrix(0.0f, 0.0f, 90.0f);
  scene.fixtures[fixture.uuid].transform.o = {0.0f, 0.0f, 0.0f};
  relative.components = {
      Component(0, 100.0, true, transform_space::TransformSpace::Local)};
  result = transform::Execute(relative, context, policy);
  assert(Near(scene.fixtures[fixture.uuid].transform.o[0], 0.0f));
  assert(Near(std::fabs(scene.fixtures[fixture.uuid].transform.o[1]), 100.0f));

  selection.fixtures.clear();
  selection.trusses = {truss.uuid};
  selection.supports = {support.uuid};
  selection.sceneObjects = {object.uuid};
  transform::Command ranged;
  ranged.components = {
      {0, {0.0, 200.0}, false, false, transform_space::TransformSpace::World}};
  result = transform::Execute(ranged, context, policy);
  assert(Near(scene.trusses[truss.uuid].transform.o[0], 0.0f));
  assert(Near(scene.supports[support.uuid].transform.o[0], 100.0f));
  assert(Near(scene.sceneObjects[object.uuid].transform.o[0], 200.0f));

  selection = {};
  result = transform::Execute(ranged, context, policy);
  assert(result.outcome == Outcome::ValidationError);
  assert(result.diagnostics.front().code ==
         "scene.transform.no_effective_targets");
  const int beforeNoTarget = host.publications;
  assert(host.publications == beforeNoTarget);

  selection.fixtures = {fixture.uuid};
  transform::Command rotation;
  rotation.kind = transform::Kind::Rotation;
  rotation.components = {Component(0, 25.0)};
  const auto origin = scene.fixtures[fixture.uuid].transform.o;
  result = transform::Execute(rotation, context, policy);
  assert(result.Success() &&
         result.request->commandId == transform::kRotationCommandId);
  assert(result.request->arguments.front().id == "x_degrees");
  assert(scene.fixtures[fixture.uuid].transform.o == origin);
  assert(Near(
      MatrixUtils::MatrixToEuler(scene.fixtures[fixture.uuid].transform)[2],
      25.0f));

  const int beforeNoop = host.publications;
  result = transform::Execute(rotation, context, policy);
  assert(result.Success() && !result.mutation.sceneChanged);
  assert(result.diagnostics.front().code == "scene.transform.noop");
  assert(host.publications == beforeNoop);

  const MvrScene exactBeforeFailure = scene;
  host.fail = true;
  relative.components = {Component(1, 75.0, true)};
  result = transform::Execute(relative, context, policy);
  assert(result.outcome == Outcome::ExecutionError);
  assert(result.diagnostics.front().code == "scene.transform.execution_failed");
  assert(scene.fixtures[fixture.uuid].transform.o ==
         exactBeforeFailure.fixtures.at(fixture.uuid).transform.o);
  host.fail = false;

  MvrScene groupedScene;
  Fixture groupedFixture;
  groupedFixture.uuid = "group-fixture";
  groupedFixture.transform = At(-100.0f);
  groupedScene.fixtures[groupedFixture.uuid] = groupedFixture;
  Truss groupedTruss;
  groupedTruss.uuid = "group-truss";
  groupedTruss.transform = At(100.0f);
  groupedScene.trusses[groupedTruss.uuid] = groupedTruss;
  scene_grouping::ObjectSelection groupedSelection;
  groupedSelection.fixtures = {groupedFixture.uuid};
  groupedSelection.trusses = {groupedTruss.uuid};
  assert(
      scene_grouping::GroupSelection(groupedScene, groupedSelection).changed);
  scene_grouping::ObjectSelection fixtureOnly;
  fixtureOnly.fixtures = {groupedFixture.uuid};
  RecordingHost groupedHost;
  ExecutionContext groupedContext{groupedScene, fixtureOnly, groupedHost};
  scene_grouping::InteractiveTransformPolicy promotedPolicy;
  promotedPolicy.promoteFixturesToGroup = true;
  transform::Command promotedMove;
  promotedMove.components = {Component(2, 50.0, true)};
  result = transform::Execute(promotedMove, groupedContext, promotedPolicy);
  assert(result.Success() && groupedHost.publications == 1);
  assert(
      Near(groupedScene.fixtures[groupedFixture.uuid].transform.o[2], 50.0f));
  assert(Near(groupedScene.trusses[groupedTruss.uuid].transform.o[2], 50.0f));

  transform::Command pivotRotation;
  pivotRotation.kind = transform::Kind::Rotation;
  pivotRotation.components = {Component(2, 90.0)};
  pivotRotation.components.front().group = true;
  pivotRotation.pivotMm = std::array<double, 3>{0.0, 0.0, 0.0};
  result = transform::Execute(pivotRotation, groupedContext, promotedPolicy);
  assert(result.Success());
  assert(
      Near(std::fabs(groupedScene.fixtures[groupedFixture.uuid].transform.o[1]),
           100.0f));

  pivotRotation.pivotMm.reset();
  result = transform::Execute(pivotRotation, groupedContext, promotedPolicy);
  assert(result.Success());

  transform::Command incrementalRotation;
  incrementalRotation.kind = transform::Kind::Rotation;
  incrementalRotation.components = {
      Component(1, 10.0, true, transform_space::TransformSpace::World),
      Component(2, 15.0, true, transform_space::TransformSpace::Local)};
  result =
      transform::Execute(incrementalRotation, groupedContext, promotedPolicy);
  assert(result.Success());
  assert(groupedHost.publications == 4);

  const auto parsed = text::ParseCommandLine("pos x 1 t 2 --local");
  assert(parsed.Success());
  const auto adapted = text::AdaptTransform(
      std::get<text::TransformCommand>(parsed.commands.front()));
  assert(adapted.kind == transform::Kind::Position);
  assert(adapted.components.front().values ==
         std::vector<double>({1000.0, 2000.0}));
  assert(adapted.components.front().space ==
         transform_space::TransformSpace::Local);

  transform::Command invalid;
  invalid.components = {
      {4, {1.0}, false, false, transform_space::TransformSpace::World}};
  const int beforeInvalid = host.publications;
  result = transform::Execute(invalid, context, policy);
  assert(result.outcome == Outcome::ValidationError);
  assert(result.diagnostics.front().code ==
         "scene.transform.invalid_component");
  assert(host.publications == beforeInvalid);
  return 0;
}
