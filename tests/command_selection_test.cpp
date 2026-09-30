#include "command/command_selection.h"
#include "command/command_selection_text_adapter.h"
#include "command/command_transform_text_adapter.h"
#include "matrixutils.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace perastage::command;

namespace {

class RecordingHost final : public ProjectMutationHost {
public:
  // Records selection-clear publication while leaving ordinary selection local.
  MutationPublication
  CommitMutation(const MvrScene &, const scene_grouping::ObjectSelection &before,
                 const std::string &undoLabel) override {
    selectionBefore = before;
    label = undoLabel;
    ++publications;
    if (fail)
      throw std::runtime_error("publication failed");
    return {true, true};
  }

  scene_grouping::ObjectSelection selectionBefore;
  std::string label;
  int publications = 0;
  bool fail = false;
};

// Creates an identity transform at the supplied X origin.
Matrix At(float x) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o[0] = x;
  return matrix;
}

// Executes parsed commands in order like the embedded Console adapter.
std::vector<Result> ExecuteLine(
    const std::string &line, ExecutionContext &context,
    const scene_grouping::InteractiveTransformPolicy &policy = {}) {
  std::vector<Result> results;
  const auto parsed = text::ParseCommandLine(line);
  assert(parsed.Success());
  for (const auto &command : parsed.commands) {
    if (const auto *selectionCommand =
            std::get_if<text::SelectionCommand>(&command)) {
      results.push_back(text::ExecuteSelection(*selectionCommand, context));
    } else if (std::holds_alternative<text::ClearCommand>(command)) {
      results.push_back(selection::ExecuteClear(context));
    } else {
      results.push_back(transform::Execute(
          text::AdaptTransform(std::get<text::TransformCommand>(command)),
          context, policy));
    }
    if (!results.back().Success())
      break;
  }
  return results;
}

// Finds a diagnostic by its stable machine code.
bool HasDiagnostic(const Result &result, const std::string &code) {
  for (const auto &diagnostic : result.diagnostics)
    if (diagnostic.code == code)
      return true;
  return false;
}

} // namespace

// Characterizes stable selection, numeric adaptation, clear, and sequencing.
int main() {
  MvrScene scene;
  Fixture first;
  first.uuid = "fixture-b";
  first.fixtureId = 1;
  first.transform = At(0.0f);
  scene.fixtures[first.uuid] = first;
  Fixture second;
  second.uuid = "fixture-a";
  second.fixtureId = 2;
  second.transform = At(100.0f);
  scene.fixtures[second.uuid] = second;
  Truss truss;
  truss.uuid = "truss-a";
  truss.unitNumber = 7;
  truss.transform = At(200.0f);
  scene.trusses[truss.uuid] = truss;
  Support support;
  support.uuid = "support-a";
  scene.supports[support.uuid] = support;
  SceneObject object;
  object.uuid = "object-a";
  scene.sceneObjects[object.uuid] = object;

  scene_grouping::ObjectSelection selected;
  selected.fixtures = {first.uuid};
  selected.trusses = {"old-truss"};
  selected.supports = {support.uuid};
  selected.sceneObjects = {object.uuid};
  RecordingHost host;
  ExecutionContext context{scene, selected, host};

  selection::Command stable;
  stable.target = selection::ObjectKind::Fixture;
  stable.operations = {{selection::OperationKind::Add,
                        {{selection::ObjectKind::Fixture, second.uuid}}}};
  Result result = selection::Execute(stable, context);
  assert(result.Success() && result.mutation.selectionChanged);
  assert(selected.fixtures ==
         std::vector<std::string>({first.uuid, second.uuid}));
  assert(selected.trusses == std::vector<std::string>({"old-truss"}));
  assert(selected.supports == std::vector<std::string>({support.uuid}));
  assert(selected.sceneObjects == std::vector<std::string>({object.uuid}));
  assert(!result.mutation.sceneChanged && !result.mutation.projectDirty);
  assert(host.publications == 0);
  assert(result.request->commandId == selection::kUpdateCommandId);
  assert(result.request->arguments[0].id == "target_kind");
  assert(result.request->arguments[2].id == "operation_kinds");

  result = selection::Execute(stable, context);
  assert(result.Success() && !result.mutation.selectionChanged);
  assert(HasDiagnostic(result, "scene.selection.noop"));

  selection::Command wrongType;
  wrongType.target = selection::ObjectKind::Fixture;
  wrongType.operations = {{selection::OperationKind::Add,
                           {{selection::ObjectKind::Truss, truss.uuid}}}};
  result = selection::Execute(wrongType, context);
  assert(result.outcome == Outcome::ValidationError);
  assert(HasDiagnostic(result, "scene.selection.invalid_object"));

  auto parsed = text::ParseCommandLine("f 1 thru 2 - 1 + 99");
  assert(parsed.Success() && parsed.commands.size() == 1);
  result = text::ExecuteSelection(
      std::get<text::SelectionCommand>(parsed.commands.front()), context);
  assert(result.Success() && result.mutation.selectionChanged);
  assert(selected.fixtures == std::vector<std::string>({second.uuid}));
  assert(HasDiagnostic(result, "scene.selection.numeric_id_missing"));
  assert(selected.supports == std::vector<std::string>({support.uuid}));
  assert(selected.sceneObjects == std::vector<std::string>({object.uuid}));

  parsed = text::ParseCommandLine("t 7");
  result = text::ExecuteSelection(
      std::get<text::SelectionCommand>(parsed.commands.front()), context);
  assert(result.Success());
  assert(selected.trusses == std::vector<std::string>({truss.uuid}));
  parsed = text::ParseCommandLine("t - 99");
  result = text::ExecuteSelection(
      std::get<text::SelectionCommand>(parsed.commands.front()), context);
  assert(result.Success() && selected.trusses.empty());
  assert(HasDiagnostic(result, "scene.selection.numeric_id_missing"));

  Fixture duplicate = first;
  duplicate.uuid = "fixture-c";
  scene.fixtures[duplicate.uuid] = duplicate;
  parsed = text::ParseCommandLine("f 1");
  const auto beforeAmbiguous = selected.fixtures;
  result = text::ExecuteSelection(
      std::get<text::SelectionCommand>(parsed.commands.front()), context);
  assert(result.Success() && selected.fixtures == beforeAmbiguous);
  assert(HasDiagnostic(result, "scene.selection.numeric_id_ambiguous"));
  scene.fixtures.erase(duplicate.uuid);

  Truss duplicateTruss = truss;
  duplicateTruss.uuid = "truss-b";
  scene.trusses[duplicateTruss.uuid] = duplicateTruss;
  selected.trusses = {truss.uuid};
  parsed = text::ParseCommandLine("t 7");
  result = text::ExecuteSelection(
      std::get<text::SelectionCommand>(parsed.commands.front()), context);
  assert(result.Success() && selected.trusses.empty());
  assert(HasDiagnostic(result, "scene.selection.numeric_id_ambiguous"));
  scene.trusses.erase(duplicateTruss.uuid);

  const size_t fixtureCount = scene.fixtures.size();
  const size_t trussCount = scene.trusses.size();
  selected.fixtures.clear();
  auto chain = ExecuteLine("f 1 pos x 1", context);
  assert(chain.size() == 2 && chain[0].mutation.selectionChanged);
  assert(chain[1].mutation.sceneChanged);
  assert(std::fabs(scene.fixtures[first.uuid].transform.o[0] - 1000.0f) <
         0.01f);
  assert(scene.fixtures[second.uuid].transform.o[0] == 100.0f);

  chain = ExecuteLine("t 7 pos x ++1", context);
  assert(chain.size() == 2 && chain[1].Success());
  assert(std::fabs(scene.trusses[truss.uuid].transform.o[0] - 1200.0f) <
         0.01f);

  chain = ExecuteLine("f 2 + 1 - 2 pos y 2", context);
  assert(chain.size() == 2);
  assert(selected.fixtures == std::vector<std::string>({first.uuid}));
  assert(std::fabs(scene.fixtures[first.uuid].transform.o[1] - 2000.0f) <
         0.01f);

  selected.fixtures = {first.uuid};
  selected.trusses = {truss.uuid};
  selected.supports = {support.uuid};
  selected.sceneObjects = {object.uuid};
  chain = ExecuteLine("f 2 clear", context);
  assert(chain.size() == 2 && chain[1].mutation.selectionChanged);
  assert(selected.fixtures.empty() && selected.trusses.empty());
  assert(selected.sceneObjects.empty());
  assert(selected.supports == std::vector<std::string>({support.uuid}));
  assert(host.label == "cli clear" && host.publications >= 1);
  assert(chain[1].mutation.undoEntryRecorded && chain[1].mutation.projectDirty);

  const int publicationsBeforeNoopClear = host.publications;
  chain = ExecuteLine("clear pos x 1", context);
  assert(chain.size() == 2);
  assert(chain[0].Success() && !chain[0].mutation.selectionChanged);
  assert(chain[0].mutation.undoEntryRecorded);
  assert(HasDiagnostic(chain[0], "scene.selection.clear_noop"));
  assert(chain[1].Success() && chain[1].mutation.sceneChanged);
  assert(std::fabs(scene.supports[support.uuid].transform.o[0] - 1000.0f) <
         0.01f);
  assert(host.publications == publicationsBeforeNoopClear + 2);

  selected.fixtures = {first.uuid};
  const auto beforeFailedClear = selected;
  host.fail = true;
  result = selection::ExecuteClear(context);
  assert(result.outcome == Outcome::ExecutionError);
  assert(selected.fixtures == beforeFailedClear.fixtures);
  assert(selected.trusses == beforeFailedClear.trusses);
  assert(selected.supports == beforeFailedClear.supports);
  assert(selected.sceneObjects == beforeFailedClear.sceneObjects);
  assert(!result.mutation.HasSemanticChanges());
  assert(!result.mutation.sceneChanged);
  assert(!result.mutation.selectionChanged);
  assert(!result.mutation.projectMetadataChanged);
  assert(!result.mutation.undoEntryRecorded);
  assert(!result.mutation.projectDirty);
  assert(HasDiagnostic(result, "scene.selection.clear_failed"));
  host.fail = false;

  assert(scene.fixtures.size() == fixtureCount);
  assert(scene.trusses.size() == trussCount);

  MvrScene groupedScene;
  Fixture groupedFixture = first;
  groupedFixture.uuid = "group-fixture";
  groupedFixture.transform = At(-50.0f);
  groupedScene.fixtures[groupedFixture.uuid] = groupedFixture;
  Truss groupedTruss = truss;
  groupedTruss.uuid = "group-truss";
  groupedTruss.transform = At(50.0f);
  groupedScene.trusses[groupedTruss.uuid] = groupedTruss;
  scene_grouping::ObjectSelection members;
  members.fixtures = {groupedFixture.uuid};
  members.trusses = {groupedTruss.uuid};
  assert(scene_grouping::GroupSelection(groupedScene, members).changed);
  scene_grouping::ObjectSelection groupedSelection;
  RecordingHost groupedHost;
  ExecutionContext groupedContext{groupedScene, groupedSelection, groupedHost};
  scene_grouping::InteractiveTransformPolicy policy;
  policy.promoteFixturesToGroup = true;
  chain = ExecuteLine("f 1 pos z +1", groupedContext, policy);
  assert(chain.size() == 2 && chain[1].Success());
  assert(std::fabs(groupedScene.fixtures[groupedFixture.uuid].transform.o[2] -
                   1000.0f) < 0.01f);
  assert(std::fabs(groupedScene.trusses[groupedTruss.uuid].transform.o[2] -
                   1000.0f) < 0.01f);
}
