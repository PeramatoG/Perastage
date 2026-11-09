#include "active_project_command_context.h"

#include "configmanager.h"
#include "mainwindow.h"
#include "selection_movement_settings.h"

// Acquires the active scene, selection, mutation host, and transform policy.
ActiveProjectCommandContext::ActiveProjectCommandContext(ConfigManager &config)
    : config_(config),
      selection_{config.GetSelectedFixtures(), config.GetSelectedTrusses(),
                 config.GetSelectedSupports(),
                 config.GetSelectedSceneObjects(), config.GetSelectedGroupObjects()},
      host_(config), execution_{config.GetScene(), selection_, host_},
      policy_(
          selection_movement_settings::LoadInteractiveTransformPolicy(config)) {
}

// Publishes selection and refreshes the normal GUI mutation boundary.
void ActiveProjectCommandContext::Publish(
    MainWindow &window, const perastage::command::MutationSummary &mutation) {
  if (!mutation.sceneChanged && !mutation.selectionChanged) return;
  config_.SetSelectedFixtures(selection_.fixtures);
  config_.SetSelectedTrusses(selection_.trusses);
  config_.SetSelectedSupports(selection_.supports);
  config_.SetSelectedSceneObjects(selection_.sceneObjects);
  config_.SetSelectedGroupObjects(selection_.groupObjects);
  if (mutation.sceneChanged)
    window.RefreshAfterToolSceneUpdate(selection_);
  else if (mutation.selectionChanged)
    window.RefreshAfterToolSelectionUpdate(selection_);
}
