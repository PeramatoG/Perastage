#include "mainwindow.h"

#include "configmanager.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "layoutviewerpanel.h"
#include "scene_grouping.h"
#include "sceneobjecttablepanel.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <string>
#include <vector>

// Refreshes scene panels for tools that manage their own selection state.
void MainWindow::RefreshAfterToolSceneUpdate() { RefreshAfterSceneChange(); }

// Refreshes scene panels while preserving the authoritative command selection.
void MainWindow::RefreshAfterToolSceneUpdate(
    const scene_grouping::ObjectSelection &selection) {
  RefreshAfterSceneChange();

  ConfigManager &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  config.SetSelectedFixtures(selection.fixtures);
  config.SetSelectedTrusses(selection.trusses);
  config.SetSelectedSupports(selection.supports);
  config.SetSelectedSceneObjects(selection.sceneObjects);

  if (fixturePanel)
    fixturePanel->SelectByUuid(selection.fixtures, false);
  if (trussPanel)
    trussPanel->SelectByUuid(selection.trusses, false);
  if (hoistPanel)
    hoistPanel->SelectByUuid(selection.supports, false);
  if (sceneObjPanel)
    sceneObjPanel->SelectByUuid(selection.sceneObjects, false);

  std::vector<std::string> mergedSelection;
  const auto appendSelection = [&](const std::vector<std::string> &source) {
    mergedSelection.insert(mergedSelection.end(), source.begin(), source.end());
  };
  appendSelection(selection.fixtures);
  appendSelection(selection.trusses);
  appendSelection(selection.supports);
  appendSelection(selection.sceneObjects);
  if (viewportPanel)
    viewportPanel->SetSelectedFixtures(mergedSelection);
  if (viewport2DPanel)
    viewport2DPanel->SetSelectedUuids(mergedSelection);
  if (layoutViewerPanel)
    layoutViewerPanel->RefreshAfterSelectionOnlyUpdate();
}
