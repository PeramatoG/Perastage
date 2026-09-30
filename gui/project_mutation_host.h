#pragma once

#include "command/command_execution.h"

class ConfigManager;

class GuiProjectMutationHost final
    : public perastage::command::ProjectMutationHost {
public:
  explicit GuiProjectMutationHost(ConfigManager &config);

  perastage::command::MutationPublication
  CommitMutation(const MvrScene &sceneBefore,
                 const scene_grouping::ObjectSelection &selectionBefore,
                 const std::string &undoLabel) override;

private:
  ConfigManager &config_;
};
