#include "command/command_mutation_transaction.h"

#include <cassert>
#include <stdexcept>

using namespace perastage::command;

namespace {

class FakeMutationHost final : public ProjectMutationHost {
public:
  // Records one publication and captures its exact pre-mutation values.
  MutationPublication
  CommitMutation(const MvrScene &sceneBefore,
                 const scene_grouping::ObjectSelection &selectionBefore,
                 const std::string &undoLabel) override {
    if (throwOnCommit)
      throw std::runtime_error("publication failed");
    ++commitCount;
    capturedProvider = sceneBefore.provider;
    capturedSelection = selectionBefore;
    capturedLabel = undoLabel;
    return {true, true};
  }

  int commitCount = 0;
  bool throwOnCommit = false;
  std::string capturedProvider;
  scene_grouping::ObjectSelection capturedSelection;
  std::string capturedLabel;
};

// Returns whether two command selections contain identical ordered buckets.
bool EqualSelection(const scene_grouping::ObjectSelection &left,
                    const scene_grouping::ObjectSelection &right) {
  return left.fixtures == right.fixtures && left.trusses == right.trusses &&
         left.supports == right.supports &&
         left.sceneObjects == right.sceneObjects;
}

} // namespace

// Verifies rollback, no-op, publication, and host-failure behavior headlessly.
int main() {
  MvrScene scene;
  scene.provider = "before";
  scene_grouping::ObjectSelection selection;
  selection.fixtures = {"fixture-before"};
  const scene_grouping::ObjectSelection selectionBefore = selection;
  FakeMutationHost host;
  ExecutionContext context{scene, selection, host};

  {
    MutationTransaction transaction(context);
    scene.provider = "provisional";
    selection.fixtures = {"fixture-provisional"};
  }
  assert(scene.provider == "before");
  assert(EqualSelection(selection, selectionBefore));
  assert(host.commitCount == 0);

  {
    MutationTransaction transaction(context);
    scene.provider = "changed";
    selection.trusses.push_back("truss-changed");
    MutationSummary changes;
    changes.sceneChanged = true;
    changes.selectionChanged = true;
    const MutationSummary committed =
        transaction.Commit(changes, "Test mutation");
    assert(committed.sceneChanged && committed.selectionChanged);
    assert(committed.undoEntryRecorded && committed.projectDirty);
  }
  assert(host.commitCount == 1);
  assert(host.capturedProvider == "before");
  assert(EqualSelection(host.capturedSelection, selectionBefore));
  assert(host.capturedLabel == "Test mutation");
  assert(scene.provider == "changed");

  {
    MutationTransaction transaction(context);
    scene.provider = "not-a-real-change";
    assert(!transaction.Commit({}, "No-op").HasSemanticChanges());
  }
  assert(scene.provider == "changed");
  assert(host.commitCount == 1);

  const scene_grouping::ObjectSelection selectionAfterCommit = selection;
  bool failed = false;
  try {
    MutationTransaction transaction(context);
    scene.provider = "execution-failed";
    selection.fixtures = {"fixture-execution-failed"};
    throw std::runtime_error("execution failed");
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
  assert(scene.provider == "changed");
  assert(EqualSelection(selection, selectionAfterCommit));
  assert(host.commitCount == 1);

  host.throwOnCommit = true;
  failed = false;
  try {
    MutationTransaction transaction(context);
    scene.provider = "failed";
    MutationSummary changes;
    changes.sceneChanged = true;
    transaction.Commit(changes, "Failed mutation");
  } catch (const std::runtime_error &) {
    failed = true;
  }
  assert(failed);
  assert(scene.provider == "changed");
  assert(host.commitCount == 1);
  return 0;
}
