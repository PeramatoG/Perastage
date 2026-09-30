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

// Compares every basis and origin component of two transforms.
bool MatrixNear(const Matrix &left, const Matrix &right) {
  for (const auto &pair :
       {std::pair{&left.u, &right.u}, std::pair{&left.v, &right.v},
        std::pair{&left.w, &right.w}, std::pair{&left.o, &right.o}})
    for (size_t index = 0; index < 3; ++index)
      if (!Near((*pair.first)[index], (*pair.second)[index]))
        return false;
  return true;
}

// Compares every ordered object bucket in two selections.
bool SelectionEqual(const scene_grouping::ObjectSelection &left,
                    const scene_grouping::ObjectSelection &right) {
  return left.fixtures == right.fixtures && left.trusses == right.trusses &&
         left.supports == right.supports &&
         left.sceneObjects == right.sceneObjects;
}

// Compares deterministic generic request projections.
bool RequestEqual(const Request &left, const Request &right) {
  if (left.commandId != right.commandId ||
      left.arguments.size() != right.arguments.size())
    return false;
  for (size_t index = 0; index < left.arguments.size(); ++index)
    if (left.arguments[index].id != right.arguments[index].id ||
        left.arguments[index].value != right.arguments[index].value)
      return false;
  return true;
}

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
  const int beforeMultiComponent = host.publications;
  Result result = transform::Execute(absolute, context, policy);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(result.mutation.undoEntryRecorded && result.mutation.projectDirty);
  assert(host.publications == beforeMultiComponent + 1 &&
         host.label == "cli pos");
  assert(Near(scene.fixtures[fixture.uuid].transform.o[0], 1000.0f));
  assert(Near(scene.fixtures[fixture.uuid].transform.o[1], 2000.0f));
  assert(Near(scene.fixtures[fixture.uuid].transform.o[2], 3000.0f));
  assert(result.request->commandId == transform::kPositionCommandId);
  assert(result.request->arguments.size() == 12);
  const std::vector<std::string> expectedPositionArguments{
      "x_millimeters", "x_relative", "x_space", "x_group",
      "y_millimeters", "y_relative", "y_space", "y_group",
      "z_millimeters", "z_relative", "z_space", "z_group"};
  for (size_t index = 0; index < expectedPositionArguments.size(); ++index)
    assert(result.request->arguments[index].id ==
           expectedPositionArguments[index]);
  assert(std::get<std::string>(result.request->arguments[2].value) == "world");
  assert(!std::get<bool>(result.request->arguments[1].value));
  assert(!std::get<bool>(result.request->arguments[3].value));
  assert(RequestEqual(*result.request, transform::BuildRequest(absolute)));

  transform::Command relative;
  relative.components = {Component(0, 50.0, true)};
  result = transform::Execute(relative, context, policy);
  assert(Near(scene.fixtures[fixture.uuid].transform.o[0], 1050.0f));
  assert(host.publications == 2);

  Matrix rotatedBasis = MatrixUtils::EulerToMatrix(90.0f, 0.0f, 0.0f);
  rotatedBasis.o = {0.0f, 0.0f, 0.0f};
  scene.fixtures[fixture.uuid].transform = rotatedBasis;
  relative.components = {Component(0, 100.0, true)};
  result = transform::Execute(relative, context, policy);
  assert(Near(scene.fixtures[fixture.uuid].transform.o[0], 100.0f));
  assert(Near(scene.fixtures[fixture.uuid].transform.o[1], 0.0f));

  scene.fixtures[fixture.uuid].transform = rotatedBasis;
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
  const int beforeNoTarget = host.publications;
  result = transform::Execute(ranged, context, policy);
  assert(result.outcome == Outcome::ValidationError);
  assert(result.diagnostics.front().code ==
         "scene.transform.no_effective_targets");
  assert(host.publications == beforeNoTarget);

  selection.fixtures = {fixture.uuid};
  transform::Command rotation;
  rotation.kind = transform::Kind::Rotation;
  Matrix scaled = MatrixUtils::EulerToMatrix(15.0f, 10.0f, 5.0f);
  for (float &value : scaled.u)
    value *= 2.0f;
  for (float &value : scaled.v)
    value *= 3.0f;
  for (float &value : scaled.w)
    value *= 4.0f;
  scaled.o = {11.0f, 22.0f, 33.0f};
  scene.fixtures[fixture.uuid].transform = scaled;
  rotation.components = {Component(0, 25.0)};
  const auto origin = scene.fixtures[fixture.uuid].transform.o;
  const auto scale = MatrixUtils::ExtractScale(scaled);
  result = transform::Execute(rotation, context, policy);
  assert(result.Success() &&
         result.request->commandId == transform::kRotationCommandId);
  assert(result.request->arguments.front().id == "x_degrees");
  assert(scene.fixtures[fixture.uuid].transform.o == origin);
  const auto rotatedScale =
      MatrixUtils::ExtractScale(scene.fixtures[fixture.uuid].transform);
  for (size_t axis = 0; axis < 3; ++axis)
    assert(Near(rotatedScale[axis], scale[axis]));
  assert(Near(
      MatrixUtils::MatrixToEuler(scene.fixtures[fixture.uuid].transform)[2],
      25.0f));

  scene.fixtures[fixture.uuid].transform = At(11.0f, 22.0f, 33.0f);
  result = transform::Execute(rotation, context, policy);
  assert(result.Success() && result.mutation.sceneChanged);
  const int beforeNoop = host.publications;
  result = transform::Execute(rotation, context, policy);
  assert(result.Success() && !result.mutation.sceneChanged);
  assert(result.diagnostics.front().code == "scene.transform.noop");
  assert(host.publications == beforeNoop);

  const Matrix rotationStart = MatrixUtils::EulerToMatrix(35.0f, 20.0f, 10.0f);
  Matrix worldStart = rotationStart;
  worldStart.o = {7.0f, 8.0f, 9.0f};
  scene.fixtures[fixture.uuid].transform = worldStart;
  transform::Command worldRotation;
  worldRotation.kind = transform::Kind::Rotation;
  worldRotation.components = {
      Component(1, 30.0, true, transform_space::TransformSpace::World)};
  result = transform::Execute(worldRotation, context, policy);
  assert(result.Success());
  const Matrix worldResult = scene.fixtures[fixture.uuid].transform;
  const Matrix rotationDelta = MatrixUtils::EulerToMatrix(0.0f, 30.0f, 0.0f);
  assert(MatrixNear(worldResult, transform_space::ApplyIncrementalRotation(
                                     worldStart, rotationDelta,
                                     transform_space::TransformSpace::World)));

  scene.fixtures[fixture.uuid].transform = worldStart;
  transform::Command localRotation = worldRotation;
  localRotation.components.front().space =
      transform_space::TransformSpace::Local;
  result = transform::Execute(localRotation, context, policy);
  assert(result.Success());
  const Matrix localResult = scene.fixtures[fixture.uuid].transform;
  assert(MatrixNear(localResult, transform_space::ApplyIncrementalRotation(
                                     worldStart, rotationDelta,
                                     transform_space::TransformSpace::Local)));
  assert(!MatrixNear(worldResult, localResult));

  const MvrScene exactBeforeFailure = scene;
  const scene_grouping::ObjectSelection selectionBeforeFailure = selection;
  const Matrix fixtureBeforeFailure = scene.fixtures[fixture.uuid].transform;
  host.fail = true;
  relative.components = {Component(1, 75.0, true)};
  result = transform::Execute(relative, context, policy);
  assert(result.outcome == Outcome::ExecutionError);
  assert(result.diagnostics.front().code == "scene.transform.execution_failed");
  assert(!result.mutation.HasSemanticChanges());
  assert(
      MatrixNear(scene.fixtures[fixture.uuid].transform, fixtureBeforeFailure));
  assert(MatrixNear(scene.fixtures[fixture.uuid].transform,
                    exactBeforeFailure.fixtures.at(fixture.uuid).transform));
  assert(SelectionEqual(selection, selectionBeforeFailure));
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
  assert(result.request->commandId == transform::kRotationCommandId);
  const std::vector<std::string> expectedRotationArguments{
      "z_degrees", "z_relative", "z_space", "z_group", "pivot_mm"};
  assert(result.request->arguments.size() == expectedRotationArguments.size());
  for (size_t index = 0; index < expectedRotationArguments.size(); ++index)
    assert(result.request->arguments[index].id ==
           expectedRotationArguments[index]);
  assert(!std::get<bool>(result.request->arguments[1].value));
  assert(std::get<std::string>(result.request->arguments[2].value) == "world");
  assert(std::get<bool>(result.request->arguments[3].value));
  assert(std::get<std::vector<double>>(result.request->arguments[4].value) ==
         std::vector<double>({0.0, 0.0, 0.0}));
  assert(RequestEqual(*result.request, transform::BuildRequest(pivotRotation)));
  assert(
      Near(std::fabs(groupedScene.fixtures[groupedFixture.uuid].transform.o[1]),
           100.0f));

  pivotRotation.pivotMm.reset();
  const auto defaultTargets = scene_grouping::BuildInteractiveTransformTargets(
      groupedScene, fixtureOnly, promotedPolicy);
  assert(defaultTargets.size() == 1);
  const auto defaultPivot = scene_grouping::GetTargetWorldTransform(
                                groupedScene, defaultTargets.front())
                                .o;
  const auto beforeDefaultPivot =
      groupedScene.fixtures[groupedFixture.uuid].transform.o;
  result = transform::Execute(pivotRotation, groupedContext, promotedPolicy);
  assert(result.Success());
  const auto afterDefaultPivot =
      groupedScene.fixtures[groupedFixture.uuid].transform.o;
  const float relativeX = beforeDefaultPivot[0] - defaultPivot[0];
  const float relativeY = beforeDefaultPivot[1] - defaultPivot[1];
  assert(Near(afterDefaultPivot[0], defaultPivot[0] + relativeY));
  assert(Near(afterDefaultPivot[1], defaultPivot[1] - relativeX));

  transform::Command groupedMultiRotation;
  groupedMultiRotation.kind = transform::Kind::Rotation;
  groupedMultiRotation.components = {
      Component(1, 10.0, true, transform_space::TransformSpace::World),
      Component(2, 15.0, true, transform_space::TransformSpace::Local)};
  const int beforeGroupedMulti = groupedHost.publications;
  result =
      transform::Execute(groupedMultiRotation, groupedContext, promotedPolicy);
  assert(result.Success());
  assert(groupedHost.publications == beforeGroupedMulti + 1);

  const auto parsed = text::ParseCommandLine("pos x 1 t 2 --local");
  assert(parsed.Success());
  const auto adapted = text::AdaptTransform(
      std::get<text::TransformCommand>(parsed.commands.front()));
  assert(adapted.kind == transform::Kind::Position);
  assert(adapted.components.front().values ==
         std::vector<double>({1000.0, 2000.0}));
  assert(adapted.components.front().space ==
         transform_space::TransformSpace::Local);
  const Request adaptedRequest = transform::BuildRequest(adapted);
  assert(adaptedRequest.arguments.size() == 4);
  assert(adaptedRequest.arguments[0].id == "x_millimeters");
  assert(std::get<std::vector<double>>(adaptedRequest.arguments[0].value) ==
         std::vector<double>({1000.0, 2000.0}));
  assert(!std::get<bool>(adaptedRequest.arguments[1].value));
  assert(std::get<std::string>(adaptedRequest.arguments[2].value) == "local");
  assert(!std::get<bool>(adaptedRequest.arguments[3].value));
  assert(RequestEqual(adaptedRequest, transform::BuildRequest(adapted)));

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
