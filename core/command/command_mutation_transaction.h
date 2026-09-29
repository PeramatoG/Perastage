#pragma once

#include "command/command_execution.h"
#include "mvrscene.h"

namespace perastage::command {

class MutationTransaction {
public:
  explicit MutationTransaction(ExecutionContext &context);
  ~MutationTransaction();

  MutationTransaction(const MutationTransaction &) = delete;
  MutationTransaction &operator=(const MutationTransaction &) = delete;

  MutationSummary Commit(const MutationSummary &changes,
                         const std::string &undoLabel);
  void Rollback();

private:
  ExecutionContext &context_;
  MvrScene sceneBefore_;
  scene_grouping::ObjectSelection selectionBefore_;
  bool finalized_ = false;
};

} // namespace perastage::command
