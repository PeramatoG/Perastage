#include "command_mutation_transaction.h"

namespace perastage::command {

// Captures the scene and selection before provisional command mutation.
MutationTransaction::MutationTransaction(ExecutionContext &context)
    : context_(context), sceneBefore_(context.scene),
      selectionBefore_(context.selection) {}

// Restores all provisional state when the transaction was not finalized.
MutationTransaction::~MutationTransaction() {
  if (!finalized_)
    Rollback();
}

// Publishes exactly one real mutation and returns its semantic summary.
MutationSummary MutationTransaction::Commit(const MutationSummary &changes,
                                            const std::string &undoLabel) {
  if (finalized_)
    return {};
  if (!changes.HasSemanticChanges()) {
    Rollback();
    return {};
  }

  try {
    const MutationPublication publication =
        context_.mutationHost.CommitMutation(sceneBefore_, selectionBefore_,
                                             undoLabel);
    MutationSummary committed = changes;
    committed.undoEntryRecorded = publication.undoEntryRecorded;
    committed.projectDirty = publication.projectDirty;
    finalized_ = true;
    return committed;
  } catch (...) {
    Rollback();
    throw;
  }
}

// Restores the exact captured state without publishing to the project host.
void MutationTransaction::Rollback() {
  if (finalized_)
    return;
  context_.scene = sceneBefore_;
  context_.selection = selectionBefore_;
  finalized_ = true;
}

} // namespace perastage::command
