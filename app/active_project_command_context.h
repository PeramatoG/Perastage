#pragma once

#include "command/command_execution.h"
#include "interactive_transform_policy.h"
#include "project_mutation_host.h"

class ConfigManager;
class MainWindow;

class ActiveProjectCommandContext {
public:
  // Acquires the active scene, selection, mutation host, and transform policy.
  explicit ActiveProjectCommandContext(ConfigManager &config);

  // Returns the semantic execution context for the active project.
  perastage::command::ExecutionContext &Execution() { return execution_; }
  // Returns the GUI-configured interactive transform policy.
  const scene_grouping::InteractiveTransformPolicy &TransformPolicy() const {
    return policy_;
  }
  // Publishes selection and refreshes the normal GUI mutation boundary.
  void Publish(MainWindow &window,
               const perastage::command::MutationSummary &mutation);

private:
  ConfigManager &config_;
  scene_grouping::ObjectSelection selection_;
  GuiProjectMutationHost host_;
  perastage::command::ExecutionContext execution_;
  scene_grouping::InteractiveTransformPolicy policy_;
};
