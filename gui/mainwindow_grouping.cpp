/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mainwindow.h"

#include "configmanager.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "hoisttablepanel.h"
#include "scene_grouping.h"
#include "sceneobjecttablepanel.h"
#include "selection_movement_settings.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

#include <cstddef>

namespace {

// Counts selected UUID references across all selectable scene tables.
std::size_t SelectionSize(const scene_grouping::ObjectSelection &selection) {
  return selection.fixtures.size() + selection.trusses.size() +
         selection.supports.size() + selection.sceneObjects.size();
}

// Builds a scene-grouping selection from the current cross-table selection
// state.
scene_grouping::ObjectSelection
BuildGroupingSelection(const ConfigManager &cfg) {
  return {.fixtures = cfg.GetSelectedFixtures(),
          .trusses = cfg.GetSelectedTrusses(),
          .supports = cfg.GetSelectedSupports(),
          .sceneObjects = cfg.GetSelectedSceneObjects()};
}

// Applies a grouping operation result to the persistent selection state.
void ApplyGroupingResultSelection(
    ConfigManager &cfg, const scene_grouping::OperationResult &result) {
  cfg.SetSelectedFixtures(result.affectedFixtures);
  cfg.SetSelectedTrusses(result.affectedTrusses);
  cfg.SetSelectedSupports(result.affectedSupports);
  cfg.SetSelectedSceneObjects(result.affectedSceneObjects);
}

// Updates table selections after grouping without triggering redundant
// selection writes.
void RefreshGroupingTableSelections(MainWindow *window,
                                    const ConfigManager &cfg) {
  (void)window;
  if (FixtureTablePanel::Instance())
    FixtureTablePanel::Instance()->SelectByUuid(cfg.GetSelectedFixtures(),
                                                false);
  if (TrussTablePanel::Instance())
    TrussTablePanel::Instance()->SelectByUuid(cfg.GetSelectedTrusses(), false);
  if (HoistTablePanel::Instance())
    HoistTablePanel::Instance()->SelectByUuid(cfg.GetSelectedSupports(), false);
  if (SceneObjectTablePanel::Instance())
    SceneObjectTablePanel::Instance()->SelectByUuid(
        cfg.GetSelectedSceneObjects(), false);
}

// Synchronizes viewport highlights with group-expanded selection membership.
void RefreshGroupingViewportHighlights(const ConfigManager &cfg) {
  const scene_grouping::ObjectSelection selection = BuildGroupingSelection(cfg);
  const std::vector<std::string> expanded =
      scene_grouping::ExpandSelectionForGroupHighlights(
          cfg.GetScene(), selection,
          selection_movement_settings::LoadInteractiveTransformPolicy(cfg));

  if (Viewer3DPanel::Instance())
    Viewer3DPanel::Instance()->SetSelectedFixtures(expanded);
  if (Viewer2DPanel::Instance())
    Viewer2DPanel::Instance()->SetSelectedUuids(expanded);
}

} // namespace

// Groups the current cross-table selection into a new MVR GroupObject.
void MainWindow::OnGroupSelection(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  const scene_grouping::ObjectSelection selection = BuildGroupingSelection(cfg);

  if (SelectionSize(selection) < 2)
    return;

  cfg.PushUndoState("group selected elements");
  scene_grouping::OperationResult result =
      scene_grouping::GroupSelection(cfg.GetScene(), selection);
  if (!result.changed) {
    cfg.Undo();
    return;
  }

  ApplyGroupingResultSelection(cfg, result);
  cfg.MarkDirty();
  RefreshAfterSceneChange(true);
  RefreshGroupingTableSelections(this, cfg);
  RefreshGroupingViewportHighlights(cfg);
}

// Ungroups selected scene entities from their direct parent GroupObjects.
void MainWindow::OnUngroupSelection(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = guiConfigServices->LegacyConfigManager();
  const scene_grouping::ObjectSelection selection = BuildGroupingSelection(cfg);

  if (SelectionSize(selection) == 0)
    return;

  cfg.PushUndoState("ungroup selected elements");
  scene_grouping::OperationResult result =
      scene_grouping::UngroupSelection(cfg.GetScene(), selection);
  if (!result.changed) {
    cfg.Undo();
    return;
  }

  ApplyGroupingResultSelection(cfg, result);
  cfg.MarkDirty();
  RefreshAfterSceneChange(true);
  RefreshGroupingTableSelections(this, cfg);
  RefreshGroupingViewportHighlights(cfg);
}
