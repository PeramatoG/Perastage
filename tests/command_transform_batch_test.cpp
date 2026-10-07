#include "command/command_transform.h"
#include "command/command_transform_batch.h"
#include "matrixutils.h"

#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace perastage::command;

namespace {

struct Row {
  std::string kind;
  std::string uuid;
  std::string component;
  std::string axis;
  double value;
  std::string mode = "absolute";
  std::string space = "world";
};

// Projects test rows to the production flat typed contract in stable order.
Request Batch(const std::vector<Row> &rows) {
  std::vector<std::string> kinds, uuids, components, axes, modes, spaces;
  std::vector<double> values;
  for (const auto &row : rows) {
    kinds.push_back(row.kind);
    uuids.push_back(row.uuid);
    components.push_back(row.component);
    axes.push_back(row.axis);
    values.push_back(row.value);
    modes.push_back(row.mode);
    spaces.push_back(row.space);
  }
  return {transform::kBatchCommandId,
          {{"target_kinds", kinds}, {"target_uuids", uuids},
           {"component_kinds", components}, {"axes", axes}, {"values", values},
           {"modes", modes}, {"spaces", spaces}}};
}

class RecordingHost final : public ProjectMutationHost {
public:
  // Captures complete Undo state or injects a publication exception.
  MutationPublication CommitMutation(
      const MvrScene &sceneBefore,
      const scene_grouping::ObjectSelection &selectionBefore,
      const std::string &undoLabel) override {
    ++attempts;
    if (fail)
      throw std::runtime_error("publication failed");
    if (failUnknown)
      throw 17;
    before = sceneBefore;
    selection = selectionBefore;
    label = undoLabel;
    ++publications;
    return {true, true};
  }
  MvrScene before;
  scene_grouping::ObjectSelection selection;
  std::string label;
  int attempts = 0;
  int publications = 0;
  bool fail = false;
  bool failUnknown = false;
};

// Builds a fixture world transform with explicit identity basis.
Matrix At(float x, float y = 0.0f, float z = 0.0f) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o = {x, y, z};
  return matrix;
}

bool Near(float left, float right) { return std::fabs(left - right) < 0.001f; }

bool MatrixEqual(const Matrix &left, const Matrix &right) {
  return left.u == right.u && left.v == right.v && left.w == right.w &&
         left.o == right.o;
}

bool MatrixNear(const Matrix &left, const Matrix &right) {
  for (const auto &pair :
       {std::pair{&left.u, &right.u}, std::pair{&left.v, &right.v},
        std::pair{&left.w, &right.w}, std::pair{&left.o, &right.o}})
    for (size_t axis = 0; axis < 3; ++axis)
      if (!Near((*pair.first)[axis], (*pair.second)[axis]))
        return false;
  return true;
}

bool SelectionEqual(const scene_grouping::ObjectSelection &left,
                    const scene_grouping::ObjectSelection &right) {
  return left.fixtures == right.fixtures && left.trusses == right.trusses &&
         left.supports == right.supports &&
         left.sceneObjects == right.sceneObjects;
}

// Verifies exact Undo/rollback of every transform and local grouping field.
template <typename Map> void AssertNodesEqual(const Map &left, const Map &right) {
  assert(left.size() == right.size());
  for (const auto &[uuid, object] : left) {
    const auto &other = right.at(uuid);
    assert(object.uuid == other.uuid);
    assert(object.parentGroupUuid == other.parentGroupUuid);
    assert(object.layer == other.layer);
    if constexpr (requires { object.name; })
      assert(object.name == other.name);
    assert(MatrixEqual(object.transform, other.transform));
    assert(MatrixEqual(object.localTransform, other.localTransform));
    if constexpr (requires { object.hasLocalTransform; })
      assert(object.hasLocalTransform == other.hasLocalTransform);
    if constexpr (requires { object.children; }) {
      assert(object.children.size() == other.children.size());
      for (size_t index = 0; index < object.children.size(); ++index) {
        assert(object.children[index].type == other.children[index].type);
        assert(object.children[index].uuid == other.children[index].uuid);
      }
    }
  }
}

// Checks scene transforms plus representative non-transform/resource state.
void AssertSceneEqual(const MvrScene &left, const MvrScene &right) {
  AssertNodesEqual(left.fixtures, right.fixtures);
  AssertNodesEqual(left.trusses, right.trusses);
  AssertNodesEqual(left.supports, right.supports);
  AssertNodesEqual(left.sceneObjects, right.sceneObjects);
  AssertNodesEqual(left.groupObjects, right.groupObjects);
  assert(left.basePath == right.basePath);
  assert(left.provider == right.provider);
  assert(left.providerVersion == right.providerVersion);
  assert(left.versionMajor == right.versionMajor);
  assert(left.versionMinor == right.versionMinor);
  assert(left.opaqueUserDataBlocks == right.opaqueUserDataBlocks);
  assert(left.runtimeResourceLeases == right.runtimeResourceLeases);
  assert(left.positions == right.positions);
  assert(left.symdefFiles == right.symdefFiles);
  assert(left.symdefTypes == right.symdefTypes);
  assert(left.symdefMatrices.size() == right.symdefMatrices.size());
  for (const auto &[uuid, matrix] : left.symdefMatrices)
    assert(MatrixEqual(matrix, right.symdefMatrices.at(uuid)));
  assert(left.symdefGeometries.size() == right.symdefGeometries.size());
  for (const auto &[uuid, geometries] : left.symdefGeometries) {
    const auto &other = right.symdefGeometries.at(uuid);
    assert(geometries.size() == other.size());
    for (size_t index = 0; index < geometries.size(); ++index) {
      assert(geometries[index].file == other[index].file);
      assert(geometries[index].geometryType == other[index].geometryType);
      assert(MatrixEqual(geometries[index].transform, other[index].transform));
    }
  }
  assert(left.layers.size() == right.layers.size());
  for (const auto &[uuid, layer] : left.layers) {
    const auto &other = right.layers.at(uuid);
    assert(layer.uuid == other.uuid && layer.name == other.name &&
           layer.color == other.color && layer.childUUIDs == other.childUUIDs);
  }
  for (const auto &[uuid, fixture] : left.fixtures) {
    const auto &other = right.fixtures.at(uuid);
    assert(fixture.instanceName == other.instanceName);
    assert(fixture.address == other.address);
    assert(fixture.visualColorHex == other.visualColorHex);
  }
}

// Supplies multiple kinds, unrelated metadata, and nontrivial prior selection.
MvrScene MakeScene() {
  MvrScene scene;
  for (const auto &uuid : {"first", "second", "unrelated"}) {
    Fixture fixture;
    fixture.uuid = uuid;
    fixture.instanceName = std::string("Fixture ") + uuid;
    fixture.address = "3.127";
    fixture.visualColorHex = "#12ABCD";
    fixture.transform = At(11.0f, 22.0f, 33.0f);
    fixture.localTransform = At(-2.0f, -3.0f, -4.0f);
    scene.fixtures.emplace(uuid, fixture);
  }
  Truss truss;
  truss.uuid = "truss";
  truss.transform = At(100.0f);
  scene.trusses.emplace(truss.uuid, truss);
  Support support;
  support.uuid = "support";
  support.transform = At(200.0f);
  scene.supports.emplace(support.uuid, support);
  SceneObject object;
  object.uuid = "object";
  object.transform = At(300.0f);
  scene.sceneObjects.emplace(object.uuid, object);
  scene.basePath = "resources";
  scene.provider = "test.provider";
  scene.providerVersion = "1.2";
  scene.opaqueUserDataBlocks = {{"foreign", "1", "<Data/>"}};
  scene.runtimeResourceLeases.push_back(std::make_shared<int>(42));
  scene.positions = {{"position", "FOH"}};
  scene.symdefFiles = {{"symdef", "fixture.glb"}};
  scene.symdefTypes = {{"symdef", "Geometry3D"}};
  scene.symdefMatrices = {{"symdef", At(42.0f)}};
  scene.symdefGeometries = {
      {"symdef", {{"fixture.glb", "Geometry3D", At(43.0f)}}}};
  Layer layer;
  layer.uuid = "layer";
  layer.name = "Lighting";
  layer.color = "#FFFFFF";
  layer.childUUIDs = {"first", "second"};
  scene.layers.emplace(layer.uuid, layer);
  return scene;
}

scene_grouping::ObjectSelection PriorSelection() {
  return {{"unrelated", "second", "unrelated"}, {"truss"}, {"support"},
          {"object"}};
}

// Proves mixed kinds/axes are one selection-preserving publication and Undo.
void MixedKindsAndUndo() {
  MvrScene scene = MakeScene();
  const MvrScene original = scene;
  auto selection = PriorSelection();
  const auto originalSelection = selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  const Request request = Batch({
      {"fixture", "first", "position", "x", 1000.0},
      {"fixture", "second", "position", "x", -1000.0},
      {"fixture", "first", "position", "y", 2000.0},
      {"fixture", "second", "rotation", "z", 90.0},
      {"fixture", "first", "position", "z", 3000.0},
      {"fixture", "second", "position", "y", -2000.0},
      {"fixture", "second", "position", "z", -3000.0},
      {"truss", "truss", "position", "z", 700.0},
      {"support", "support", "position", "x", 800.0},
      {"scene_object", "object", "rotation", "y", 45.0}});
  const Result result = transform::ExecuteBatch(request, context);
  assert(result.Success() && result.mutation.sceneChanged);
  assert(result.mutation.undoEntryRecorded && result.mutation.projectDirty);
  assert(!result.mutation.selectionChanged && host.publications == 1);
  assert(host.label == "batch transform");
  assert(result.request->commandId == transform::kBatchCommandId);
  assert(result.request->arguments.size() == request.arguments.size());
  for (size_t index = 0; index < request.arguments.size(); ++index) {
    assert(result.request->arguments[index].id == request.arguments[index].id);
    assert(result.request->arguments[index].value == request.arguments[index].value);
  }
  assert((scene.fixtures.at("first").transform.o ==
          std::array<float, 3>{1000.0f, 2000.0f, 3000.0f}));
  assert((scene.fixtures.at("second").transform.o ==
          std::array<float, 3>{-1000.0f, -2000.0f, -3000.0f}));
  assert(Near(MatrixUtils::MatrixToEuler(scene.fixtures.at("second").transform)[0],
              90.0f));
  assert(Near(scene.trusses.at("truss").transform.o[2], 700.0f));
  assert(Near(scene.supports.at("support").transform.o[0], 800.0f));
  assert(SelectionEqual(selection, originalSelection));
  assert(MatrixEqual(scene.fixtures.at("unrelated").transform,
                     original.fixtures.at("unrelated").transform));
  AssertSceneEqual(host.before, original);
  assert(SelectionEqual(host.selection, originalSelection));
  // The host snapshot is the complete state used by application Undo.
  scene = host.before;
  selection = host.selection;
  AssertSceneEqual(scene, original);
  assert(SelectionEqual(selection, originalSelection));
}

// Supplies independent circle positions as data, with no layout implementation.
void CircleFixtureData() {
  constexpr std::array<std::array<double, 3>, 8> positions{{
      {1000.0, 0.0, 2000.0}, {707.10678, 707.10678, 2000.0},
      {0.0, 1000.0, 2000.0}, {-707.10678, 707.10678, 2000.0},
      {-1000.0, 0.0, 2000.0}, {-707.10678, -707.10678, 2000.0},
      {0.0, -1000.0, 2000.0}, {707.10678, -707.10678, 2000.0}}};
  MvrScene scene;
  std::vector<Row> rows;
  for (size_t index = 0; index < positions.size(); ++index) {
    Fixture fixture;
    fixture.uuid = "circle-" + std::to_string(index);
    fixture.transform = At(0.0f);
    scene.fixtures.emplace(fixture.uuid, fixture);
    for (size_t axis = 0; axis < 3; ++axis)
      rows.push_back({"fixture", fixture.uuid, "position",
                      axis == 0 ? "x" : axis == 1 ? "y" : "z",
                      positions[index][axis]});
  }
  scene_grouping::ObjectSelection selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  const Request request = Batch(rows);
  auto result = transform::ExecuteBatch(request, context);
  assert(result.Success() && host.publications == 1);
  for (size_t index = 0; index < positions.size(); ++index)
    for (size_t axis = 0; axis < 3; ++axis)
      assert(Near(scene.fixtures.at("circle-" + std::to_string(index))
                      .transform.o[axis], static_cast<float>(positions[index][axis])));
  const MvrScene beforeNoop = scene;
  result = transform::ExecuteBatch(request, context);
  assert(result.Success() && !result.mutation.HasSemanticChanges());
  assert(!result.mutation.undoEntryRecorded && !result.mutation.projectDirty);
  assert(result.diagnostics.front().code == "scene.transform.batch.noop");
  assert(host.publications == 1);
  AssertSceneEqual(scene, beforeNoop);
}

// Characterizes both relative spaces and absolute Euler/scale compatibility.
void ExistingTransformSemantics() {
  for (const std::string component : {"position", "rotation"})
    for (int axis = 0; axis < 3; ++axis)
      for (const std::string mode : {"absolute", "relative"})
        for (const std::string space : {"world", "local"}) {
          MvrScene batchScene = MakeScene();
          Matrix start = MatrixUtils::EulerToMatrix(35.0f, 20.0f, 10.0f);
          for (float &value : start.u) value *= 2.0f;
          for (float &value : start.v) value *= 3.0f;
          for (float &value : start.w) value *= 4.0f;
          start.o = {15.0f, 25.0f, 35.0f};
          batchScene.fixtures.at("first").transform = start;
          MvrScene legacyScene = batchScene;
          auto batchSelection = PriorSelection();
          const auto originalSelection = batchSelection;
          scene_grouping::ObjectSelection legacySelection{{"first"}, {}, {}, {}};
          RecordingHost batchHost, legacyHost;
          ExecutionContext batchContext{batchScene, batchSelection, batchHost};
          ExecutionContext legacyContext{legacyScene, legacySelection, legacyHost};
          const char *axisName = axis == 0 ? "x" : axis == 1 ? "y" : "z";
          auto result = transform::ExecuteBatch(
              Batch({{"fixture", "first", component, axisName, 57.0, mode, space}}),
              batchContext);
          transform::Command legacy;
          legacy.kind = component == "position" ? transform::Kind::Position
                                                 : transform::Kind::Rotation;
          legacy.components = {{axis, {57.0}, mode == "relative", false,
                                 space == "local" ? transform_space::TransformSpace::Local
                                                  : transform_space::TransformSpace::World}};
          const auto expected = transform::Execute(legacy, legacyContext, {});
          assert(result.Success() && expected.Success());
          assert(MatrixNear(batchScene.fixtures.at("first").transform,
                            legacyScene.fixtures.at("first").transform));
          assert(SelectionEqual(batchSelection, originalSelection));
        }
}

// Checks repeated mixed components against sequential established semantics.
void OrderedRows() {
  MvrScene scene = MakeScene();
  MvrScene expected = scene;
  auto selection = PriorSelection();
  scene_grouping::ObjectSelection expectedSelection{{"first"}, {}, {}, {}};
  RecordingHost host, expectedHost;
  ExecutionContext context{scene, selection, host};
  ExecutionContext expectedContext{expected, expectedSelection, expectedHost};
  const std::vector<Row> rows{
      {"fixture", "first", "position", "x", 10.0},
      {"fixture", "first", "position", "x", 5.0, "relative"},
      {"fixture", "first", "rotation", "z", 90.0},
      {"fixture", "first", "position", "y", 12.0, "relative", "local"},
      {"fixture", "first", "rotation", "x", 15.0, "relative", "local"},
      {"fixture", "first", "position", "x", 20.0}};
  for (const auto &row : rows) {
    transform::Command command;
    command.kind = row.component == "position" ? transform::Kind::Position
                                               : transform::Kind::Rotation;
    command.components = {{row.axis == "x" ? 0 : row.axis == "y" ? 1 : 2,
                            {row.value}, row.mode == "relative", false,
                            row.space == "local" ? transform_space::TransformSpace::Local
                                                 : transform_space::TransformSpace::World}};
    assert(transform::Execute(command, expectedContext, {}).Success());
  }
  assert(transform::ExecuteBatch(Batch(rows), context).Success());
  assert(host.publications == 1);
  assert(MatrixNear(scene.fixtures.at("first").transform,
                    expected.fixtures.at("first").transform));
}

// Checks complete-request validation without touching any scene/host state.
void InvalidRequests() {
  MvrScene scene = MakeScene();
  const auto original = scene;
  auto selection = PriorSelection();
  const auto originalSelection = selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  const auto valid = Batch({{"fixture", "first", "position", "x", 999.0},
                            {"fixture", "second", "position", "y", 777.0}});
  auto reject = [&](const Request &request, const char *code) {
    const auto result = transform::ExecuteBatch(request, context);
    assert(result.outcome == Outcome::ValidationError);
    assert(result.diagnostics.front().code == code);
    assert(!result.mutation.HasSemanticChanges());
    assert(!result.mutation.undoEntryRecorded && !result.mutation.projectDirty);
    assert(host.attempts == 0);
    AssertSceneEqual(scene, original);
    assert(SelectionEqual(selection, originalSelection));
  };
  reject(Batch({}), "scene.transform.batch.invalid_arguments");
  for (size_t index = 0; index < valid.arguments.size(); ++index) {
    Request missing = valid;
    missing.arguments.erase(missing.arguments.begin() + index);
    reject(missing, "scene.transform.batch.invalid_arguments");
    Request malformed = valid;
    malformed.arguments[index].value = false;
    reject(malformed, "scene.transform.batch.invalid_arguments");
    Request misaligned = valid;
    if (index == 4)
      misaligned.arguments[index].value = std::vector<double>{1.0};
    else
      std::get<std::vector<std::string>>(misaligned.arguments[index].value).pop_back();
    reject(misaligned, "scene.transform.batch.invalid_arguments");
  }
  Request duplicate = valid;
  duplicate.arguments.push_back(valid.arguments.front());
  reject(duplicate, "scene.transform.batch.invalid_arguments");
  Request unknown = valid;
  unknown.arguments.push_back({"surprise", true});
  reject(unknown, "scene.transform.batch.invalid_arguments");
  Request wrongCommand = valid;
  wrongCommand.commandId = transform::kPositionCommandId;
  reject(wrongCommand, "scene.transform.batch.invalid_arguments");
  for (size_t index : {0, 2, 3, 5, 6}) {
    Request invalid = valid;
    std::get<std::vector<std::string>>(invalid.arguments[index].value)[1] = "invalid";
    reject(invalid, index == 0 ? "scene.transform.batch.invalid_target"
                              : "scene.transform.batch.invalid_component");
  }
  for (const auto &uuid : {"", "missing", "support"}) {
    Request invalid = valid;
    std::get<std::vector<std::string>>(invalid.arguments[1].value)[1] = uuid;
    reject(invalid, "scene.transform.batch.invalid_target");
  }
  for (double invalidValue : {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()}) {
    Request invalid = valid;
    std::get<std::vector<double>>(invalid.arguments[4].value)[1] = invalidValue;
    reject(invalid, "scene.transform.batch.invalid_component");
  }
}

// Checks preview overflow and both publication exception paths roll back.
void ExecutionFailureAndNoop() {
  MvrScene scene = MakeScene();
  const auto original = scene;
  auto selection = PriorSelection();
  const auto originalSelection = selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  const Request request = Batch({{"fixture", "first", "position", "x", 999.0},
                                 {"fixture", "second", "position", "y", 777.0}});
  for (bool unknown : {false, true}) {
    host.fail = !unknown;
    host.failUnknown = unknown;
    const auto result = transform::ExecuteBatch(request, context);
    assert(result.outcome == Outcome::ExecutionError);
    assert(result.diagnostics.front().code == "scene.transform.batch.execution_failed");
    assert(!result.mutation.HasSemanticChanges() && host.publications == 0);
    assert(!result.mutation.undoEntryRecorded && !result.mutation.projectDirty);
    AssertSceneEqual(scene, original);
    assert(SelectionEqual(selection, originalSelection));
  }
  host.fail = host.failUnknown = false;
  const int attempts = host.attempts;
  for (const std::string component : {"position", "rotation"}) {
    Request overflow = request;
    std::get<std::vector<double>>(overflow.arguments[4].value)[1] =
        std::numeric_limits<double>::max();
    std::get<std::vector<std::string>>(overflow.arguments[2].value)[1] = component;
    const auto result = transform::ExecuteBatch(overflow, context);
    assert(result.outcome == Outcome::ExecutionError && host.attempts == attempts);
    AssertSceneEqual(scene, original);
  }
  // Each input is finite and float-representable, but the preview sum is not.
  const double large = std::numeric_limits<float>::max();
  const auto overflow = transform::ExecuteBatch(
      Batch({{"fixture", "first", "position", "x", large},
             {"fixture", "first", "position", "x", large, "relative"}}), context);
  assert(overflow.outcome == Outcome::ExecutionError && host.attempts == attempts);
  AssertSceneEqual(scene, original);
  // No-op ignores provisional initialization of absent local transform metadata.
  const auto noop = transform::ExecuteBatch(
      Batch({{"fixture", "first", "position", "x", 11.0},
             {"fixture", "second", "position", "z", 0.0, "relative"}}), context);
  assert(noop.Success() && !noop.mutation.HasSemanticChanges());
  assert(!noop.mutation.undoEntryRecorded && !noop.mutation.projectDirty);
  assert(host.attempts == attempts);
  AssertSceneEqual(scene, original);
  // Root local storage is not used by group synchronization and may be stale.
  scene.fixtures.at("first").hasLocalTransform = true;
  const MvrScene staleRoot = scene;
  const auto staleNoop = transform::ExecuteBatch(
      Batch({{"fixture", "first", "position", "x", 11.0}}), context);
  assert(staleNoop.Success() && !staleNoop.mutation.HasSemanticChanges());
  assert(host.attempts == attempts);
  AssertSceneEqual(scene, staleRoot);
  scene = original;
  const auto cancel = transform::ExecuteBatch(
      Batch({{"fixture", "first", "position", "x", 20.0, "relative"},
             {"fixture", "first", "position", "x", -20.0, "relative"}}), context);
  assert(cancel.Success() && !cancel.mutation.HasSemanticChanges());
  assert(host.attempts == attempts);
  AssertSceneEqual(scene, original);
}

// Proves exact children, explicit groups, descendants, and ordered mixed scopes.
void ExactChildrenAndGroups() {
  MvrScene scene = MakeScene();
  scene_grouping::ObjectSelection members{{"first", "second"}, {"truss"}, {}, {}};
  const auto groupResult = scene_grouping::GroupSelection(scene, members);
  assert(groupResult.changed);
  const std::string group = groupResult.groupUuid;
  auto selection = PriorSelection();
  const auto originalSelection = selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  const auto beforeChild = scene;
  const auto childMove = transform::ExecuteBatch(
      Batch({{"truss", "truss", "position", "z", 500.0, "relative"}}), context);
  assert(childMove.Success() && host.publications == 1);
  assert(Near(scene.trusses.at("truss").transform.o[2], 500.0f));
  assert(MatrixEqual(scene.groupObjects.at(group).transform,
                     beforeChild.groupObjects.at(group).transform));
  assert(MatrixEqual(scene.fixtures.at("first").transform,
                     beforeChild.fixtures.at("first").transform));
  assert(MatrixEqual(scene.fixtures.at("second").transform,
                     beforeChild.fixtures.at("second").transform));
  assert(SelectionEqual(selection, originalSelection));
  const auto beforeGroup = scene;
  const auto groupMove = transform::ExecuteBatch(
      Batch({{"group", group, "position", "x", 100.0, "relative"}}), context);
  assert(groupMove.Success() && host.publications == 2);
  for (const auto &uuid : {"first", "second"})
    assert(Near(scene.fixtures.at(uuid).transform.o[0],
                beforeGroup.fixtures.at(uuid).transform.o[0] + 100.0f));
  assert(Near(scene.trusses.at("truss").transform.o[0],
              beforeGroup.trusses.at("truss").transform.o[0] + 100.0f));
  AssertSceneEqual(host.before, beforeGroup);
  // Parent returns to the same world transform while one exact child changes.
  const auto beforeMixed = scene;
  const auto mixed = transform::ExecuteBatch(
      Batch({{"group", group, "position", "x", 50.0, "relative"},
             {"fixture", "first", "position", "y", 25.0, "relative"},
             {"group", group, "position", "x", -50.0, "relative"}}), context);
  assert(mixed.Success() && mixed.mutation.sceneChanged && host.publications == 3);
  assert(MatrixNear(scene.groupObjects.at(group).transform,
                    beforeMixed.groupObjects.at(group).transform));
  assert(Near(scene.fixtures.at("first").transform.o[1],
              beforeMixed.fixtures.at("first").transform.o[1] + 25.0f));
  assert(MatrixNear(scene.fixtures.at("second").transform,
                    beforeMixed.fixtures.at("second").transform));
  assert(SelectionEqual(selection, originalSelection));
  AssertSceneEqual(host.before, beforeMixed);
  scene = host.before;
  AssertSceneEqual(scene, beforeMixed);
}

// Reuses group synchronization for nested rotations and overflow rollback.
void NestedGroupsAndDescendantOverflow() {
  MvrScene scene = MakeScene();
  const auto inner = scene_grouping::GroupSelection(
      scene, {{"first", "second"}, {"truss"}, {}, {}});
  assert(inner.changed);
  // Structural grouping resolves the truss to its complete existing group.
  const auto outer = scene_grouping::GroupSelection(
      scene, {{}, {"truss"}, {"support"}, {}});
  assert(outer.changed);
  assert(scene.groupObjects.at(inner.groupUuid).parentGroupUuid == outer.groupUuid);
  auto selection = PriorSelection();
  const auto originalSelection = selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  MvrScene expected = scene;
  const scene_grouping::SceneTransformTarget target{
      MvrNodeType::GroupObject, outer.groupUuid};
  const Matrix original = scene_grouping::GetTargetWorldTransform(expected, target);
  const Matrix rotated = transform_space::ApplyIncrementalRotation(
      original, MatrixUtils::EulerToMatrix(90.0f, 0.0f, 0.0f),
      transform_space::TransformSpace::World);
  scene_grouping::SetTargetWorldTransform(expected, target, rotated);
  const auto result = transform::ExecuteBatch(
      Batch({{"group", outer.groupUuid, "rotation", "z", 90.0, "relative"}}),
      context);
  assert(result.Success() && host.publications == 1);
  AssertSceneEqual(scene, expected);
  assert(SelectionEqual(selection, originalSelection));

  MvrScene extreme;
  Fixture fixture;
  fixture.uuid = "extreme";
  fixture.parentGroupUuid = "inner";
  fixture.transform = At(std::numeric_limits<float>::max());
  fixture.localTransform = fixture.transform;
  fixture.hasLocalTransform = true;
  extreme.fixtures.emplace(fixture.uuid, fixture);
  GroupObject group;
  group.uuid = "group";
  group.transform = group.localTransform = At(0.0f);
  group.children = {{MvrNodeType::GroupObject, "inner"}};
  extreme.groupObjects.emplace(group.uuid, group);
  GroupObject nested;
  nested.uuid = "inner";
  nested.parentGroupUuid = group.uuid;
  nested.transform = nested.localTransform = At(0.0f);
  nested.children = {{MvrNodeType::Fixture, fixture.uuid}};
  extreme.groupObjects.emplace(nested.uuid, nested);
  const auto before = extreme;
  RecordingHost extremeHost;
  ExecutionContext extremeContext{extreme, selection, extremeHost};
  const auto overflow = transform::ExecuteBatch(
      Batch({{"group", "group", "position", "x",
               static_cast<double>(std::numeric_limits<float>::max()) / 2.0,
               "relative"}}), extremeContext);
  assert(overflow.outcome == Outcome::ExecutionError);
  assert(extremeHost.attempts == 0 && !overflow.mutation.HasSemanticChanges());
  AssertSceneEqual(extreme, before);
  assert(SelectionEqual(selection, originalSelection));
}

} // namespace

int main() {
  MixedKindsAndUndo();
  CircleFixtureData();
  ExistingTransformSemantics();
  OrderedRows();
  InvalidRequests();
  ExecutionFailureAndNoop();
  ExactChildrenAndGroups();
  NestedGroupsAndDescendantOverflow();
  return 0;
}
