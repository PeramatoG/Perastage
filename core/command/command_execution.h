#pragma once

#include "command/command_contract.h"
#include "scene_grouping.h"

#include <string>

class MvrScene;

namespace perastage::command {

struct MutationPublication {
  bool undoEntryRecorded = false;
  bool projectDirty = false;
};

class ProjectMutationHost {
public:
  // Destroys a project mutation host through its neutral interface.
  virtual ~ProjectMutationHost() = default;

  virtual MutationPublication
  CommitMutation(const MvrScene &sceneBefore,
                 const scene_grouping::ObjectSelection &selectionBefore,
                 const std::string &undoLabel) = 0;
};

struct ExecutionContext {
  MvrScene &scene;
  scene_grouping::ObjectSelection &selection;
  ProjectMutationHost &mutationHost;
};

} // namespace perastage::command
