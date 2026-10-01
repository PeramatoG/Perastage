#include "command/command_text_processor.h"
#include "matrixutils.h"
#include "mvrscene.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace perastage::command;

namespace {

class RecordingHost final : public ProjectMutationHost {
public:
  // Records semantic transaction publication and optionally rejects it.
  MutationPublication CommitMutation(const MvrScene &,
                                     const scene_grouping::ObjectSelection &,
                                     const std::string &) override {
    ++publications;
    if (fail)
      throw std::runtime_error("publication failed");
    return {true, true};
  }

  int publications = 0;
  bool fail = false;
};

// Creates an identity transform at the supplied world position.
Matrix At(float x, float y = 0.0f) {
  Matrix matrix = MatrixUtils::Identity();
  matrix.o = {x, y, 0.0f};
  return matrix;
}

// Finds a stable diagnostic code in one semantic result.
bool HasDiagnostic(const Result &result, const std::string &code) {
  for (const Diagnostic &diagnostic : result.diagnostics)
    if (diagnostic.code == code)
      return true;
  return false;
}

} // namespace

// Characterizes ordered neutral text processing and structured change reports.
int main() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture-a";
  fixture.fixtureId = 1;
  fixture.transform = At(0.0f);
  scene.fixtures.emplace(fixture.uuid, fixture);
  Truss truss;
  truss.uuid = "truss-a";
  truss.unitNumber = 7;
  truss.transform = At(100.0f);
  scene.trusses.emplace(truss.uuid, truss);

  scene_grouping::ObjectSelection selection;
  RecordingHost host;
  ExecutionContext context{scene, selection, host};
  scene_grouping::InteractiveTransformPolicy policy;

  auto execution =
      text::ProcessCommandLine("f 1 pos x 1 rot z ++90", context, policy);
  assert(execution.Success() && execution.records.size() == 3);
  assert(std::holds_alternative<text::SelectionCommand>(
      execution.records[0].command));
  assert(execution.records[0].result.request->commandId ==
         "scene.selection.update");
  assert(execution.records[0].selectionAfter.fixtures ==
         std::vector<std::string>{fixture.uuid});
  assert(execution.records[1].result.request->commandId ==
         "scene.transform.position");
  assert(execution.records[2].result.request->commandId ==
         "scene.transform.rotation");
  assert(execution.mutation.selectionChanged &&
         execution.mutation.sceneChanged);
  assert(execution.mutation.undoEntryRecorded &&
         execution.mutation.projectDirty);
  assert(std::fabs(scene.fixtures.at(fixture.uuid).transform.o[0] - 1000.0f) <
         0.01f);
  assert(host.publications == 2);

  execution = text::ProcessCommandLine("t 7", context, policy);
  assert(execution.Success() && execution.records.size() == 1);
  assert(selection.trusses == std::vector<std::string>{truss.uuid});
  assert(execution.records.front().result.mutation.selectionChanged);

  execution = text::ProcessCommandLine("clear", context, policy);
  assert(execution.Success() && execution.records.size() == 1);
  assert(selection.fixtures.empty() && selection.trusses.empty());
  assert(execution.records.front().result.request->commandId ==
         "scene.selection.clear");
  assert(execution.mutation.selectionChanged &&
         execution.mutation.projectDirty);

  execution = text::ProcessCommandLine("unknown", context, policy);
  assert(!execution.Success() && execution.stopped &&
         execution.records.empty());
  assert(execution.parseDiagnostics.size() == 1);
  assert(execution.parseDiagnostics.front().phase == DiagnosticPhase::Parse);

  execution = text::ProcessCommandLine("pos x 2 rot y 10", context, policy);
  assert(!execution.Success() && execution.stopped);
  assert(execution.records.size() == 1);
  assert(execution.records.front().result.outcome == Outcome::ValidationError);
  assert(HasDiagnostic(execution.records.front().result,
                       "scene.transform.no_effective_targets"));

  selection.fixtures = {fixture.uuid};
  const Matrix beforeFailure = scene.fixtures.at(fixture.uuid).transform;
  host.fail = true;
  execution = text::ProcessCommandLine("pos y 3 rot x 5", context, policy);
  assert(!execution.Success() && execution.records.size() == 1);
  assert(execution.records.front().result.outcome == Outcome::ExecutionError);
  assert(HasDiagnostic(execution.records.front().result,
                       "scene.transform.execution_failed"));
  assert(!execution.mutation.HasSemanticChanges());
  assert(scene.fixtures.at(fixture.uuid).transform.o == beforeFailure.o);

  host.fail = false;
  selection.fixtures.clear();
  const int publicationsBeforeOrderedStop = host.publications;
  execution = text::ProcessCommandLine("f 1 pos x 2 f - 1 pos y 3 rot z 10",
                                       context, policy);
  assert(!execution.Success() && execution.stopped);
  assert(execution.records.size() == 4);
  assert(execution.records[0].result.request->commandId ==
         "scene.selection.update");
  assert(execution.records[1].result.request->commandId ==
         "scene.transform.position");
  assert(execution.records[2].result.request->commandId ==
         "scene.selection.update");
  assert(execution.records[3].result.request->commandId ==
         "scene.transform.position");
  assert(execution.records[3].result.outcome == Outcome::ValidationError);
  assert(!execution.records[3].result.mutation.HasSemanticChanges());
  assert(selection.fixtures.empty());
  assert(std::fabs(scene.fixtures.at(fixture.uuid).transform.o[0] - 2000.0f) <
         0.01f);
  assert(execution.mutation.selectionChanged &&
         execution.mutation.sceneChanged);
  assert(execution.mutation.undoEntryRecorded &&
         execution.mutation.projectDirty);
  assert(host.publications == publicationsBeforeOrderedStop + 1);

  selection.fixtures = {fixture.uuid};
  const int publicationsBeforePartialParse = host.publications;
  execution = text::ProcessCommandLine("clear pos x invalid", context, policy);
  assert(!execution.Success() && execution.stopped);
  assert(execution.records.size() == 1);
  assert(execution.records.front().result.request->commandId ==
         "scene.selection.clear");
  assert(execution.parseDiagnostics.size() == 1);
  assert(execution.parseDiagnostics.front().phase == DiagnosticPhase::Parse);
  assert(selection.fixtures.empty());
  assert(execution.mutation.selectionChanged &&
         execution.mutation.undoEntryRecorded);
  assert(host.publications == publicationsBeforePartialParse + 1);
}
