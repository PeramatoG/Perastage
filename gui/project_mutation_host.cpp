#include "project_mutation_host.h"

#include "configmanager.h"
#include "project_fixture_identity.h"

// Creates an application mutation publisher backed by the active project.
GuiProjectMutationHost::GuiProjectMutationHost(ConfigManager &config)
    : config_(config), selectionOrderBefore_(config.GetOrderedSelection()) {}

// Publishes the exact pre-command scene and selection as one Undo entry.
perastage::command::MutationPublication GuiProjectMutationHost::CommitMutation(
    const MvrScene &sceneBefore,
    const scene_grouping::ObjectSelection &selectionBefore,
    const std::string &undoLabel) {
  SelectionState selection;
  selection.SetSelectedFixtures(selectionBefore.fixtures);
  selection.SetSelectedTrusses(selectionBefore.trusses);
  selection.SetSelectedSupports(selectionBefore.supports);
  selection.SetSelectedSceneObjects(selectionBefore.sceneObjects);
  selection.SetSelectedGroupObjects(selectionBefore.groupObjects);
  selection.RestoreOrderedSelection(selectionOrderBefore_);
  config_.PushUndoSnapshot(
      sceneBefore, selection,
      config_.GetValue(project_identity::kFixtureLabelOverridesConfigKey),
      undoLabel);
  config_.NotifyChange(ConfigManager::Change::Scene);
  return {true, config_.IsDirty()};
}
